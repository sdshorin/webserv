#pragma once

#include <poll.h>
#include <list>
#include <deque>
#include <map>

#include "IOSocket.h"

class Cluster;

struct SocketEvent
{
	typedef void (Cluster::*handler_type)(IOSocket*, size_t);

	IOSocket *socket;
	std::deque<handler_type> handlers; /// get SIGPIPE - try to write to closed socket
};

class SocketContainer
{
private:
	typedef std::map<int, IOSocket> socket_wrapper_type;

	std::vector<pollfd> m_SocketsPdfs;
	socket_wrapper_type m_SocketWrappers;

public:
	void AddSocket(IOSocket socket, const short *events, size_t events_size);
	void DelSocket(const IOSocket *deleted_socket, size_t socket_pdfs_idx);

	void AddEvent(size_t sock_ind, const short *events, size_t events_size);
	void DelEvent(size_t sock_ind, const short *events, size_t events_size);

	pollfd *GetPdfs();
	size_t GetPdfsSize() const;
	SocketEvent GetEvent(size_t idx);

private:
	size_t AddToPdfs(int socket, const short *events, size_t events_size);
	void DelFromPdfs(std::size_t deleted_idx);
};
