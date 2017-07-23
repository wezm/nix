#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

// pub fn test_scm_rights() {
//     use nix::sys::uio::IoVec;
//     use nix::unistd::{pipe, read, write, close};
//     use nix::sys::socket::{socketpair, sendmsg, recvmsg,
//                            AddressFamily, SockType, SockFlag,
//                            ControlMessage, CmsgSpace, MsgFlags,
//                            MSG_TRUNC, MSG_CTRUNC};
// 
//     let (fd1, fd2) = socketpair(AddressFamily::Unix, SockType::Stream, 0,
//                                 SockFlag::empty())
//                      .unwrap();
//     let (r, w) = pipe().unwrap();
//     let mut received_r: Option<RawFd> = None;
// 
//     {
//         let iov = [IoVec::from_slice(b"hello")];
//         let fds = [r];
//         let cmsg = ControlMessage::ScmRights(&fds);
//         assert_eq!(sendmsg(fd1, &iov, &[cmsg], MsgFlags::empty(), None).unwrap(), 5);
//         close(r).unwrap();
//         close(fd1).unwrap();
//     }
// 
//     {
//         let mut buf = [0u8; 5];
//         let iov = [IoVec::from_mut_slice(&mut buf[..])];
//         let mut cmsgspace: CmsgSpace<[RawFd; 1]> = CmsgSpace::new();
//         let msg = recvmsg(fd2, &iov, Some(&mut cmsgspace), MsgFlags::empty()).unwrap();
// 
//         for cmsg in msg.cmsgs() {
//             if let ControlMessage::ScmRights(fd) = cmsg {
//                 assert_eq!(received_r, None);
//                 assert_eq!(fd.len(), 1);
//                 received_r = Some(fd[0]);
//             } else {
//                 panic!("unexpected cmsg");
//             }
//         }
//         assert!(!msg.flags.intersects(MSG_TRUNC | MSG_CTRUNC));
//         close(fd2).unwrap();
//     }
// 
//     let received_r = received_r.expect("Did not receive passed fd");
//     // Ensure that the received file descriptor works
//     write(w, b"world").unwrap();
//     let mut buf = [0u8; 5];
//     read(received_r, &mut buf).unwrap();
//     assert_eq!(&buf[..], b"world");
//     close(received_r).unwrap();
//     close(w).unwrap();
// }
int err(int status, const char *msg) {
	fprintf(stderr, "error: %s\n", msg);
	exit(status);
}

int main(int argc, char **argv) {
   // int socketpair(int d, int type, int protocol, int sv[2]);
   int fds[2];
   if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1)
	   err(1, "socketpair");
   int fd1 = fds[0];
   int fd2 = fds[1];

   int pipe_fds[2];
   if (pipe(pipe_fds) == -1)
	   err(1, "pipe");
   int r = pipe_fds[0];
   int w = pipe_fds[1];

   {
	   struct msghdr    msg;
	   struct cmsghdr  *cmsg;
	   union {
		   struct cmsghdr hdr;
		   unsigned char    buf[CMSG_SPACE(sizeof(int))];
	   } cmsgbuf;

	   // iov_base, iov_len
	   char *hello = "hello";
	   struct iovec iov = {
		   hello,
		   strlen(hello)
	   };
	   int fds[] = {r};

	   memset(&msg, 0, sizeof(msg));
	   printf("sizeof(msghdr) = %lu\n", sizeof(struct msghdr));
	   printf("sizeof(cmsghdr) = %lu, _ALIGNBYTES = %lu\n", sizeof(struct cmsghdr), _ALIGNBYTES);
	   msg.msg_control = &cmsgbuf.buf;
	   msg.msg_controllen = sizeof(cmsgbuf.buf);

	   cmsg = CMSG_FIRSTHDR(&msg);
	   cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	   cmsg->cmsg_level = SOL_SOCKET;
	   cmsg->cmsg_type = SCM_RIGHTS;
	   *(int *)CMSG_DATA(cmsg) = r;

	   if (sendmsg(fd1, &msg, 0) == -1)
		   err(1, "sendmsg");
   }

   {
	   struct msghdr    msg;
	   struct cmsghdr  *cmsg;
	   union {
		   struct cmsghdr hdr;
		   unsigned char    buf[CMSG_SPACE(sizeof(int))];
	   } cmsgbuf;

	   memset(&msg, 0, sizeof(msg));
	   msg.msg_control = &cmsgbuf.buf;
	   msg.msg_controllen = sizeof(cmsgbuf.buf);

	   if (recvmsg(fd2, &msg, 0) == -1)
		   err(1, "recvmsg");
	   if ((msg.msg_flags & MSG_TRUNC) || (msg.msg_flags & MSG_CTRUNC))
		   err(1, "control message truncated");
	   for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	       cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		   if (cmsg->cmsg_len == CMSG_LEN(sizeof(int)) &&
		       cmsg->cmsg_level == SOL_SOCKET &&
		       cmsg->cmsg_type == SCM_RIGHTS) {
			   int received_r = *(int *)CMSG_DATA(cmsg);
			   /* Do something with the descriptor. */
			   printf("got fd from msg: %d\n", received_r);
		   }
	   }
   }

   return 0;
}
