#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

#include "nfsmount.h"
#include "sunrpc.h"

struct portmap_call
{
	struct rpc_call rpc;
	__u32 program;
	__u32 version;
	__u32 proto;
	__u32 port;
};

struct portmap_reply
{
	struct rpc_reply rpc;
	__u32 port;
};

static struct portmap_call call = {
	.rpc.program = __constant_htonl(RPC_PMAP_PROGRAM),
	.rpc.prog_vers = __constant_htonl(RPC_PMAP_VERSION),
	.rpc.proc = __constant_htonl(PMAP_PROC_GETPORT),
};

__u32 portmap(__u32 server, __u32 program, __u32 version, __u32 proto)
{
	struct portmap_reply reply;
	struct client *clnt;
	struct rpc rpc;
	__u32 port = 0;

	if ((clnt = tcp_client(server, RPC_PMAP_PORT, 0)) == NULL) {
		if ((clnt = udp_client(server, RPC_PMAP_PORT, 0)) == NULL) {
			goto bail;
		}
	}

	call.program = htonl(program);
	call.version = htonl(version);
	call.proto = htonl(proto);

	rpc.call = (struct rpc_call *) &call;
	rpc.call_len = sizeof(call);
	rpc.reply = (struct rpc_reply *) &reply;
	rpc.reply_len = sizeof(reply);

	if (rpc_call(clnt, &rpc) < 0)
		goto bail;

	if (rpc.reply_len < sizeof(reply)) {
		fprintf(stderr, "incomplete reply: %d < %d\n",
			rpc.reply_len, sizeof(reply));
		goto bail;
	}

	port = ntohl(reply.port);

 bail:
	DEBUG(("Port for %d/%d[%s]: %d\n", program, version,
	       proto == IPPROTO_TCP ? "tcp" : "udp", port));

	if (clnt) {
		client_free(clnt);
	}

	return port;
}
