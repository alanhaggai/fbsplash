#include <sys/mount.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/nfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nfsmount.h"
#include "sunrpc.h"

static __u32 mount_port;

struct mount_call
{
	struct rpc_call rpc;
	__u32 path_len;
	char path[0];
};

#define NFS_MAXFHSIZE 64

struct nfs_fh
{
	__u32 size;
	char data[NFS_MAXFHSIZE];
} __attribute__((packed));

struct mount_reply
{
	struct rpc_reply reply;
	__u32 status;
	struct nfs_fh fh;
} __attribute__((packed));

#define MNT_REPLY_MINSIZE (sizeof(struct rpc_reply) + sizeof(__u32))

static void get_ports(__u32 server, const struct nfs_mount_data *data)
{
	__u32 nfs_ver, mount_ver;
	__u32 proto;

	if (data->flags & NFS_MOUNT_VER3) {
		nfs_ver = NFS3_VERSION;
		mount_ver = NFS_MNT3_VERSION;
	} else {
		nfs_ver = NFS2_VERSION;
		mount_ver = NFS_MNT_VERSION;
	}

	proto = (data->flags & NFS_MOUNT_TCP) ? IPPROTO_TCP : IPPROTO_UDP;

	if (nfs_port == 0) {
		nfs_port = portmap(server, NFS_PROGRAM, nfs_ver,
				   proto);
		if (nfs_port == 0) {
			if (proto == IPPROTO_TCP) {
				struct in_addr addr = { server };
				fprintf(stderr, "NFS over TCP not "
					"available from %s\n",
					inet_ntoa(addr));
				exit(1);
			}
			nfs_port = NFS_PORT;
		}
	}

	if (mount_port == 0) {
		mount_port = portmap(server, NFS_MNT_PROGRAM, mount_ver,
				     proto);
		if (mount_port == 0)
			mount_port = MOUNT_PORT;
	}
}

static inline int pad_len(int len)
{
	return (len + 3) & ~3;
}

static inline void dump_params(__u32 server,
			       const char *path,
			       const struct nfs_mount_data *data)
{
#ifdef NFS_DEBUG
	struct in_addr addr = { server };

	printf("NFS params:\n");
	printf("  server = %s, path = \"%s\", ", inet_ntoa(addr), path);
	printf("version = %d, proto = %s\n",
	       data->flags & NFS_MOUNT_VER3 ? 3 : 2,
	       (data->flags & NFS_MOUNT_TCP) ? "tcp" : "udp");
	printf("  mount_port = %d, nfs_port = %d, flags = %08x\n",
	       mount_port, nfs_port, data->flags);
	printf("  rsize = %d, wsize = %d, timeo = %d, retrans = %d\n",
	       data->rsize, data->wsize, data->timeo, data->retrans);
	printf("  acreg (min, max) = (%d, %d), acdir (min, max) = (%d, %d)\n",
	       data->acregmin, data->acregmax, data->acdirmin, data->acdirmax);
	printf("  soft = %d, intr = %d, posix = %d, nocto = %d, noac = %d\n",
	       (data->flags & NFS_MOUNT_SOFT) != 0,
	       (data->flags & NFS_MOUNT_INTR) != 0,
	       (data->flags & NFS_MOUNT_POSIX) != 0,
	       (data->flags & NFS_MOUNT_NOCTO) != 0,
	       (data->flags & NFS_MOUNT_NOAC) != 0);
#endif
}

static inline void dump_fh(const char *data, int len)
{
#ifdef NFS_DEBUG
	int i = 0;
	int max = len - (len % 8);

	printf("Root file handle: %d bytes\n", NFS2_FHSIZE);

	while (i < max) {
		int j;

		printf("  %4d:  ", i);
		for (j = 0; j < 4; j++) {
			printf("%02x %02x %02x %02x  ",
			       data[i] & 0xff, data[i + 1] & 0xff,
			       data[i + 2] & 0xff, data[i + 3] & 0xff);
		}
		i += j;
		printf("\n");
	}
#endif
}

static struct mount_reply mnt_reply;

static int mount_call(__u32 proc, __u32 version,
		      const char *path,
		      struct client *clnt)
{
	struct mount_call *mnt_call = NULL;
	size_t path_len, call_len;
	struct rpc rpc;
	int ret = 0;

	path_len = strlen(path);
	call_len = sizeof(*mnt_call) + pad_len(path_len);

	if ((mnt_call = malloc(call_len)) == NULL) {
		perror("malloc");
		goto bail;
	}

	memset(mnt_call, 0, sizeof(*mnt_call));

	mnt_call->rpc.program = __constant_htonl(NFS_MNT_PROGRAM);
	mnt_call->rpc.prog_vers = __constant_htonl(version);
	mnt_call->rpc.proc = __constant_htonl(proc);
	mnt_call->path_len = htonl(path_len);
	memcpy(mnt_call->path, path, path_len);

	rpc.call = (struct rpc_call *) mnt_call;
	rpc.call_len = call_len;
	rpc.reply = (struct rpc_reply *) &mnt_reply;
	rpc.reply_len = sizeof(mnt_reply);

	if (rpc_call(clnt, &rpc) < 0)
		goto bail;

	if (rpc.reply_len < MNT_REPLY_MINSIZE) {
		fprintf(stderr, "incomplete reply: %zu < %zu\n",
			rpc.reply_len, MNT_REPLY_MINSIZE);
		goto bail;
	}

	if (mnt_reply.status != 0) {
		fprintf(stderr, "mount call failed: %d\n",
			ntohl(mnt_reply.status));
		goto bail;
	}

	goto done;

 bail:
	ret = -1;

 done:
	if (mnt_call) {
		free(mnt_call);
	}

	return ret;
}

static int mount_v2(const char *path,
		    struct nfs_mount_data *data,
		    struct client *clnt)
{
	int ret = mount_call(MNTPROC_MNT, NFS_MNT_VERSION, path, clnt);

	if (ret == 0) {
		dump_fh((const char *) &mnt_reply.fh, NFS2_FHSIZE);

		data->root.size = NFS_FHSIZE;
		memcpy(data->root.data, &mnt_reply.fh, NFS_FHSIZE);
		memcpy(data->old_root.data, &mnt_reply.fh, NFS_FHSIZE);
	}

	return ret;
}

static inline int umount_v2(const char *path, struct client *clnt)
{
	return mount_call(MNTPROC_UMNT, NFS_MNT_VERSION, path, clnt);
}

static int mount_v3(const char *path,
		    struct nfs_mount_data *data,
		    struct client *clnt)
{
	int ret = mount_call(MNTPROC_MNT, NFS_MNT3_VERSION, path, clnt);

	if (ret == 0) {
		size_t fhsize = ntohl(mnt_reply.fh.size);

		dump_fh((const char *) &mnt_reply.fh.data, fhsize);

		memset(data->old_root.data, 0, NFS_FHSIZE);
		memset(&data->root, 0, sizeof(data->root));
		data->root.size = fhsize;
		memcpy(&data->root.data, mnt_reply.fh.data, fhsize);
		data->flags |= NFS_MOUNT_VER3;
	}

	return ret;
}

static inline int umount_v3(const char *path, struct client *clnt)
{
	return mount_call(MNTPROC_UMNT, NFS_MNT3_VERSION, path, clnt);
}

int nfs_mount(const char *pathname, const char *hostname,
	      __u32 server, const char *rem_path, const char *path,
	      struct nfs_mount_data *data)
{
	struct client *clnt = NULL;
	struct sockaddr_in addr;
	char mounted = 0;
	int sock = -1;
	int ret = 0;
	int mountflags;

	get_ports(server, data);

	dump_params(server, path, data);

	if (data->flags & NFS_MOUNT_TCP) {
		clnt = tcp_client(server, mount_port, CLI_RESVPORT);
	} else {
		clnt = udp_client(server, mount_port, CLI_RESVPORT);
	}

	if (clnt == NULL) {
		goto bail;
	}

	if (data->flags & NFS_MOUNT_VER3) {
		ret = mount_v3(rem_path, data, clnt);
	} else {
		ret = mount_v2(rem_path, data, clnt);
	}

	if (ret == -1) {
		goto bail;
	}
	mounted = 1;

	if (data->flags & NFS_MOUNT_TCP) {
		sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	} else {
		sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}

	if (sock == -1) {
		perror("socket");
		goto bail;
	}

	if (bindresvport(sock, 0) == -1) {
		perror("bindresvport");
		goto bail;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = server;
	addr.sin_port = htons(nfs_port);
	memcpy(&data->addr, &addr, sizeof(data->addr));

	strncpy(data->hostname, hostname, sizeof(data->hostname));

	data->fd = sock;

	mountflags = (data->flags & NFS_MOUNT_KLIBC_RONLY) ? MS_RDONLY : 0;
	data->flags = data->flags & NFS_MOUNT_FLAGMASK;
	ret = mount(pathname, path, "nfs", mountflags, data);

	if (ret == -1) {
		perror("mount");
		goto bail;
	}

	DEBUG(("Mounted %s on %s\n", pathname, path));

	goto done;

 bail:
	if (mounted) {
		if (data->flags & NFS_MOUNT_VER3) {
			umount_v3(path, clnt);
		} else {
			umount_v2(path, clnt);
		}
	}

	ret = -1;

 done:
	if (clnt) {
		client_free(clnt);
	}

	if (sock != -1) {
		close(sock);
	}

	return ret;
}

