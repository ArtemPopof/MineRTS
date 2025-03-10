/*
Minetest
Copyright (C) 2013, 2017 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "mesh_generator_thread.h"
#include "settings.h"
#include "profiler.h"
#include "client.h"
#include "mapblock.h"
#include "map.h"
#include "util/directiontables.h"

/*
	CachedMapBlockData
*/

CachedMapBlockData::~CachedMapBlockData()
{
	assert(refcount_from_queue == 0);

	delete[] data;
}

/*
	QueuedMeshUpdate
*/

QueuedMeshUpdate::~QueuedMeshUpdate()
{
	delete data;
}

/*
	MeshUpdateQueue
*/

MeshUpdateQueue::MeshUpdateQueue(Client *client):
	m_client(client),
	m_next_cache_cleanup(0)
{
	m_cache_enable_shaders = g_settings->getBool("enable_shaders");
	m_cache_smooth_lighting = g_settings->getBool("smooth_lighting");
	m_meshgen_block_cache_size = g_settings->getS32("meshgen_block_cache_size");
}

MeshUpdateQueue::~MeshUpdateQueue()
{
	MutexAutoLock lock(m_mutex);

	for (auto &i : m_cache) {
		delete i.second;
	}

	for (QueuedMeshUpdate *q : m_queue) {
		delete q;
	}
}

bool MeshUpdateQueue::addBlock(Map *map, v3s16 p, bool ack_block_to_server, bool urgent)
{
	MutexAutoLock lock(m_mutex);

	cleanupCache();

	/*
		Cache the block data (force-update the center block, don't update the
		neighbors but get them if they aren't already cached)
	*/
	std::vector<CachedMapBlockData*> cached_blocks;
	size_t cache_hit_counter = 0;
	CachedMapBlockData *cached_block = cacheBlock(map, p, FORCE_UPDATE);
	if (!cached_block->data)
		return false; // nothing to update
	cached_blocks.reserve(3*3*3);
	cached_blocks.push_back(cached_block);
	for (v3s16 dp : g_26dirs)
		cached_blocks.push_back(cacheBlock(map, p + dp,
				SKIP_UPDATE_IF_ALREADY_CACHED,
				&cache_hit_counter));
	g_profiler->avg("MeshUpdateQueue: MapBlocks from cache [%]",
			100.0f * cache_hit_counter / cached_blocks.size());

	/*
		Mark the block as urgent if requested
	*/
	if (urgent)
		m_urgents.insert(p);

	/*
		Find if block is already in queue.
		If it is, update the data and quit.
	*/
	for (QueuedMeshUpdate *q : m_queue) {
		if (q->p == p) {
			// NOTE: We are not adding a new position to the queue, thus
			//       refcount_from_queue stays the same.
			if(ack_block_to_server)
				q->ack_block_to_server = true;
			q->crack_level = m_client->getCrackLevel();
			q->crack_pos = m_client->getCrackPos();
			q->urgent |= urgent;
			return true;
		}
	}

	/*
		Add the block
	*/
	QueuedMeshUpdate *q = new QueuedMeshUpdate;
	q->p = p;
	q->ack_block_to_server = ack_block_to_server;
	q->crack_level = m_client->getCrackLevel();
	q->crack_pos = m_client->getCrackPos();
	q->urgent = urgent;
	m_queue.push_back(q);

	// This queue entry is a new reference to the cached blocks
	for (CachedMapBlockData *cached_block : cached_blocks) {
		cached_block->refcount_from_queue++;
	}
	return true;
}

// Returned pointer must be deleted
// Returns NULL if queue is empty
QueuedMeshUpdate *MeshUpdateQueue::pop()
{
	MutexAutoLock lock(m_mutex);

	bool must_be_urgent = !m_urgents.empty();
	for (std::vector<QueuedMeshUpdate*>::iterator i = m_queue.begin();
			i != m_queue.end(); ++i) {
		QueuedMeshUpdate *q = *i;
		if (must_be_urgent && m_urgents.count(q->p) == 0)
			continue;
		// Make sure no two threads are processing the same mapblock, as that causes racing conditions
		if (m_inflight_blocks.find(q->p) != m_inflight_blocks.end())
			continue;
		m_queue.erase(i);
		m_urgents.erase(q->p);
		m_inflight_blocks.insert(q->p);
		fillDataFromMapBlockCache(q);
		return q;
	}
	return NULL;
}

void MeshUpdateQueue::done(v3s16 pos)
{
	MutexAutoLock lock(m_mutex);
	m_inflight_blocks.erase(pos);
}

CachedMapBlockData* MeshUpdateQueue::cacheBlock(Map *map, v3s16 p, UpdateMode mode,
			size_t *cache_hit_counter)
{
	CachedMapBlockData *cached_block = nullptr;
	auto it = m_cache.find(p);

	if (it != m_cache.end()) {
		cached_block = it->second;

		if (mode == SKIP_UPDATE_IF_ALREADY_CACHED) {
			if (cache_hit_counter)
				(*cache_hit_counter)++;
			return cached_block;
		}
	}

	if (!cached_block) {
		// Not yet in cache
		cached_block = new CachedMapBlockData();
		m_cache[p] = cached_block;
	}

	MapBlock *b = map->getBlockNoCreateNoEx(p);
	if (b) {
		if (!cached_block->data)
			cached_block->data =
					new MapNode[MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE];
		memcpy(cached_block->data, b->getData(),
				MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE * sizeof(MapNode));
	} else {
		delete[] cached_block->data;
		cached_block->data = nullptr;
	}
	return cached_block;
}

CachedMapBlockData* MeshUpdateQueue::getCachedBlock(const v3s16 &p)
{
	auto it = m_cache.find(p);
	if (it != m_cache.end()) {
		return it->second;
	}
	return NULL;
}

void MeshUpdateQueue::fillDataFromMapBlockCache(QueuedMeshUpdate *q)
{
	MeshMakeData *data = new MeshMakeData(m_client, m_cache_enable_shaders);
	q->data = data;

	data->fillBlockDataBegin(q->p);

	std::time_t t_now = std::time(0);

	// Collect data for 3*3*3 blocks from cache
	for (v3s16 dp : g_27dirs) {
		v3s16 p = q->p + dp;
		CachedMapBlockData *cached_block = getCachedBlock(p);
		if (cached_block) {
			cached_block->refcount_from_queue--;
			cached_block->last_used_timestamp = t_now;
			if (cached_block->data)
				data->fillBlockData(dp, cached_block->data);
		}
	}

	data->setCrack(q->crack_level, q->crack_pos);
	data->setSmoothLighting(m_cache_smooth_lighting);
}

void MeshUpdateQueue::cleanupCache()
{
	const int mapblock_kB = MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE *
			sizeof(MapNode) / 1000;
	g_profiler->avg("MeshUpdateQueue MapBlock cache size kB",
			mapblock_kB * m_cache.size());

	// Iterating the entire cache can get pretty expensive so don't do it too often
	{
		constexpr int cleanup_interval = 250;
		const u64 now = porting::getTimeMs();
		if (m_next_cache_cleanup > now)
			return;
		m_next_cache_cleanup = now + cleanup_interval;
	}

	// The cache size is kept roughly below cache_soft_max_size, not letting
	// anything get older than cache_seconds_max or deleted before 2 seconds.
	const int cache_seconds_max = 10;
	const int cache_soft_max_size = m_meshgen_block_cache_size * 1000 / mapblock_kB;
	int cache_seconds = MYMAX(2, cache_seconds_max -
			m_cache.size() / (cache_soft_max_size / cache_seconds_max));

	int t_now = time(0);

	for (auto it = m_cache.begin(); it != m_cache.end(); ) {
		CachedMapBlockData *cached_block = it->second;
		if (cached_block->refcount_from_queue == 0 &&
				cached_block->last_used_timestamp < t_now - cache_seconds) {
			it = m_cache.erase(it);
			delete cached_block;
		} else {
			++it;
		}
	}
}

/*
	MeshUpdateWorkerThread
*/

MeshUpdateWorkerThread::MeshUpdateWorkerThread(MeshUpdateQueue *queue_in, MeshUpdateManager *manager, v3s16 *camera_offset) :
		UpdateThread("Mesh"), m_queue_in(queue_in), m_manager(manager), m_camera_offset(camera_offset)
{
	m_generation_interval = g_settings->getU16("mesh_generation_interval");
	m_generation_interval = rangelim(m_generation_interval, 0, 50);
}

void MeshUpdateWorkerThread::doUpdate()
{
	QueuedMeshUpdate *q;
	while ((q = m_queue_in->pop())) {
		if (m_generation_interval)
			sleep_ms(m_generation_interval);
		ScopeProfiler sp(g_profiler, "Client: Mesh making (sum)");

		MapBlockMesh *mesh_new = new MapBlockMesh(q->data, *m_camera_offset);

		

		MeshUpdateResult r;
		r.p = q->p;
		r.mesh = mesh_new;
		r.solid_sides = get_solid_sides(q->data);
		r.ack_block_to_server = q->ack_block_to_server;
		r.urgent = q->urgent;

		m_manager->putResult(r);
		m_queue_in->done(q->p);
		delete q;
	}
}

/*
	MeshUpdateManager
*/

MeshUpdateManager::MeshUpdateManager(Client *client):
	m_queue_in(client)
{
	int number_of_threads = rangelim(g_settings->getS32("mesh_generation_threads"), 0, 8);

	// Automatically use 33% of the system cores for mesh generation, max 4
	if (number_of_threads == 0)
		number_of_threads = MYMIN(4, Thread::getNumberOfProcessors() / 3);
	
	// use at least one thread
	number_of_threads = MYMAX(1, number_of_threads);
	infostream << "MeshUpdateManager: using " << number_of_threads << " threads" << std::endl;

	for (int i = 0; i < number_of_threads; i++)
		m_workers.push_back(std::make_unique<MeshUpdateWorkerThread>(&m_queue_in, this, &m_camera_offset));
}

void MeshUpdateManager::updateBlock(Map *map, v3s16 p, bool ack_block_to_server,
		bool urgent, bool update_neighbors)
{
	static thread_local const bool many_neighbors =
			g_settings->getBool("smooth_lighting")
			&& !g_settings->getFlag("performance_tradeoffs");
	if (!m_queue_in.addBlock(map, p, ack_block_to_server, urgent)) {
		warningstream << "Update requested for non-existent block at ("
				<< p.X << ", " << p.Y << ", " << p.Z << ")" << std::endl;
		return;
	}
	if (update_neighbors) {
		if (many_neighbors) {
			for (v3s16 dp : g_26dirs)
				m_queue_in.addBlock(map, p + dp, false, urgent);
		} else {
			for (v3s16 dp : g_6dirs)
				m_queue_in.addBlock(map, p + dp, false, urgent);
		}
	}
	deferUpdate();
}

void MeshUpdateManager::putResult(const MeshUpdateResult &result)
{
	if (result.urgent)
		m_queue_out_urgent.push_back(result);
	else
		m_queue_out.push_back(result);
}

bool MeshUpdateManager::getNextResult(MeshUpdateResult &r)
{
	if (!m_queue_out_urgent.empty()) {
		r = m_queue_out_urgent.pop_frontNoEx();
		return true;
	}

	if (!m_queue_out.empty()) {
		r = m_queue_out.pop_frontNoEx();
		return true;
	}

	return false;
}

void MeshUpdateManager::deferUpdate()
{
	for (auto &thread : m_workers)
		thread->deferUpdate();
}

void MeshUpdateManager::start()
{
	for (auto &thread: m_workers)
		thread->start();
}

void MeshUpdateManager::stop()
{
	for (auto &thread: m_workers)
		thread->stop();
}

void MeshUpdateManager::wait()
{
	for (auto &thread: m_workers)
		thread->wait();
}

bool MeshUpdateManager::isRunning()
{
	for (auto &thread: m_workers)
		if (thread->isRunning())
			return true;
	return false;
}
