/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
#ifndef SSDB_BACKEND_SYNC_H_
#define SSDB_BACKEND_SYNC_H_

#include "include.h"
#include <vector>
#include <string>
#include <map>

#include "ssdb/ssdb_impl.h"
#include "ssdb/log_reader_writer.h"
#include "net/link.h"
#include "util/thread.h"

class SSDBServer;

class BackendSync{
private:
	struct Client;
	struct CopySnapshot;

private:
	std::vector<Client *> clients;
	std::vector<Client *> clients_tmp;

	struct run_arg{
		const Link *link;
		const BackendSync *backend;
	};
	volatile bool thread_quit;
	static void* _run_thread(void *arg);
	Mutex mutex;
	std::map<pthread_t, Client *> workers;
	SSDBImpl *ssdb;
	int sync_speed;

	SSDBServer *owner;

private:
	std::map<std::string, CopySnapshot *> snapshots;
	uint32_t snapshot_timeout;

	static void* _timer_thread(void *arg);
	void clear_timeout_snapshot();

public:
	CopySnapshot *last_snapshot(const std::string &host);
	CopySnapshot *create_snapshot(const std::string &host);
	void release_last_snapshot(const std::string &host);
	void mark_snapshot(const std::string &host, int status);

public:
	BackendSync(SSDBServer *owner, SSDBImpl *ssdb, int sync_speed, uint32_t snapshot_timeout=3600);
	~BackendSync();
	void proc(const Link *link);                                                   /* sync all */
	void reset();

	std::vector<std::string> stats();
};

struct BackendSync::CopySnapshot {
	enum Status {
		ABORT = 1,
		ACTIVE = 2,
	};

	const rocksdb::Snapshot *snapshot;
	int status;
	time_t last_active;
	uint64_t binlog_seq;
};

struct BackendSync::Client{
	static const int INIT = 0;
	static const int OUT_OF_SYNC = 1;
	static const int COPY = 2;
	static const int SYNC = 4;

	int status;
	Link *link;
	uint64_t last_seq;
	uint64_t last_noop_seq;
	std::string last_key;
	BackendSync *backend;

	std::string host;
	int peer_server_port;

	Iterator *iter;

	// binlog
	LogReader logreader;
	LogFile logfile;

	Client(BackendSync *backend);
	~Client();
	void init();
	void reset();
	void noop();
	int pre_snapshot();
	int post_snapshot();
	int pre_binlog();
	int post_binlog() {return 0;}
	int seek_binlog(uint64_t seq);
	int next_binlog(const std::string &nextfile);
	int read(LogEvent *event);
	int copy();

	std::string stats();
};

#endif
