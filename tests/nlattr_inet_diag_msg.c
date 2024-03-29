/*
 * Copyright (c) 2017 JingPiao Chen <chenjingpiao@gmail.com>
 * Copyright (c) 2017-2021 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include "test_nlattr.h"
#include <linux/inet_diag.h>
#include <linux/sock_diag.h>


#ifndef HAVE_STRUCT_TCP_DIAG_MD5SIG
struct tcp_diag_md5sig {
	__u8	tcpm_family;
	__u8	tcpm_prefixlen;
	__u16	tcpm_keylen;
	__be32	tcpm_addr[4];
	__u8	tcpm_key[80 /* TCP_MD5SIG_MAXKEYLEN */];
};
#endif

static const char * const sk_meminfo_strs[] = {
	"SK_MEMINFO_RMEM_ALLOC",
	"SK_MEMINFO_RCVBUF",
	"SK_MEMINFO_WMEM_ALLOC",
	"SK_MEMINFO_SNDBUF",
	"SK_MEMINFO_FWD_ALLOC",
	"SK_MEMINFO_WMEM_QUEUED",
	"SK_MEMINFO_OPTMEM",
	"SK_MEMINFO_BACKLOG",
	"SK_MEMINFO_DROPS",
};

static const char address[] = "10.11.12.13";

static void
init_inet_diag_msg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = SOCK_DIAG_BY_FAMILY,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct inet_diag_msg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct inet_diag_msg, msg,
		.idiag_family = AF_INET,
		.idiag_state = TCP_LISTEN,
		.id.idiag_if = ifindex_lo()
	);

	if (!inet_pton(AF_INET, address, msg->id.idiag_src) ||
	    !inet_pton(AF_INET, address, msg->id.idiag_dst))
		perror_msg_and_skip("inet_pton");
}

static void
print_inet_diag_msg(const unsigned int msg_len)
{
	printf("{nlmsg_len=%u, nlmsg_type=SOCK_DIAG_BY_FAMILY"
	       ", nlmsg_flags=NLM_F_DUMP, nlmsg_seq=0, nlmsg_pid=0}"
	       ", {idiag_family=AF_INET, idiag_state=TCP_LISTEN"
	       ", idiag_timer=0, idiag_retrans=0"
	       ", id={idiag_sport=htons(0), idiag_dport=htons(0)"
	       ", idiag_src=inet_addr(\"%s\")"
	       ", idiag_dst=inet_addr(\"%s\")"
	       ", idiag_if=" IFINDEX_LO_STR
	       ", idiag_cookie=[0, 0]}"
	       ", idiag_expires=0, idiag_rqueue=0, idiag_wqueue=0"
	       ", idiag_uid=0, idiag_inode=0}",
	       msg_len, address, address);
}

static void
print_uint(const unsigned int *p, size_t i)
{
	if (i >= ARRAY_SIZE(sk_meminfo_strs))
		printf("[%zu /* SK_MEMINFO_??? */", i);
	else
		printf("[%s", sk_meminfo_strs[i]);

	printf("] = %u", *p);
}

static const struct {
	struct tcp_diag_md5sig val;
	const char *str;
} md5sig_vecs[] = {
	{ { 0 },
	  "{tcpm_family=AF_UNSPEC, tcpm_prefixlen=0, tcpm_keylen=0"
	  ", tcpm_addr=\"\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00"
	  "\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\", tcpm_key=\"\"}" },
	{ { AF_INET, 0x42, 1, { BE_LE(0xdeadface, 0xcefaedde) } },
	  "{tcpm_family=AF_INET, tcpm_prefixlen=66, tcpm_keylen=1"
	  ", tcpm_addr=inet_addr(\"222.237.250.206\")"
	  ", tcpm_key=\"\\x00\"}" },
	{ { AF_INET6, 0xbe, 42,
	    { BE_LE(0xdeadface, 0xcefaadde), BE_LE(0xcafe0000, 0xfeca),
	      BE_LE(0xface, 0xcefa0000), BE_LE(0xbadc0ded, 0xed0cdcba) },
	    "OH HAI THAR\0\1\2\3\4\5\6\7\3779876543210abcdefghijklmnopqrstuv" },
	  "{tcpm_family=AF_INET6, tcpm_prefixlen=190, tcpm_keylen=42"
	  ", inet_pton(AF_INET6, \"dead:face:cafe::face:badc:ced\", &tcpm_addr)"
	  ", tcpm_key=\"\\x4f\\x48\\x20\\x48\\x41\\x49\\x20\\x54\\x48\\x41"
	  "\\x52\\x00\\x01\\x02\\x03\\x04\\x05\\x06\\x07\\xff\\x39\\x38\\x37"
	  "\\x36\\x35\\x34\\x33\\x32\\x31\\x30\\x61\\x62\\x63\\x64\\x65\\x66"
	  "\\x67\\x68\\x69\\x6a\\x6b\\x6c\"}" },
	{ { 46, 0, 45067,
	    { BE_LE(0xdeadface, 0xcefaadde), BE_LE(0xcafe0000, 0xfeca),
	      BE_LE(0xface, 0xcefa0000), BE_LE(0xbadc0ded, 0xed0cdcba) },
	    "OH HAI THAR\0\1\2\3\4\5\6\7\3779876543210abcdefghijklmnopqrstuv"
	    "xyz0123456789ABCDEFGHIJKLMNO" },
	  "{tcpm_family=0x2e /* AF_??? */, tcpm_prefixlen=0, tcpm_keylen=45067"
	  ", tcpm_addr=\"\\xde\\xad\\xfa\\xce\\xca\\xfe\\x00\\x00"
	  "\\x00\\x00\\xfa\\xce\\xba\\xdc\\x0c\\xed\""
	  ", tcpm_key=\"\\x4f\\x48\\x20\\x48\\x41\\x49\\x20\\x54\\x48\\x41"
	  "\\x52\\x00\\x01\\x02\\x03\\x04\\x05\\x06\\x07\\xff\\x39\\x38\\x37"
	  "\\x36\\x35\\x34\\x33\\x32\\x31\\x30\\x61\\x62\\x63\\x64\\x65\\x66"
	  "\\x67\\x68\\x69\\x6a\\x6b\\x6c\\x6d\\x6e\\x6f\\x70\\x71\\x72\\x73"
	  "\\x74\\x75\\x76\\x78\\x79\\x7a\\x30\\x31\\x32\\x33\\x34\\x35\\x36"
	  "\\x37\\x38\\x39\\x41\\x42\\x43\\x44\\x45\\x46\\x47\\x48\\x49\\x4a"
	  "\\x4b\\x4c\\x4d\\x4e\\x4f\"}" },
};

static void
print_md5sig(const struct tcp_diag_md5sig *p, size_t i)
{
	printf("%s", md5sig_vecs[i].str);
}

int
main(void)
{
	skip_if_unavailable("/proc/self/fd/");

	static const struct inet_diag_meminfo minfo = {
		.idiag_rmem = 0xfadcacdb,
		.idiag_wmem = 0xbdabcada,
		.idiag_fmem = 0xbadbfafb,
		.idiag_tmem = 0xfdacdadf
	};
	static const struct tcpvegas_info vegas = {
		.tcpv_enabled = 0xfadcacdb,
		.tcpv_rttcnt = 0xbdabcada,
		.tcpv_rtt = 0xbadbfafb,
		.tcpv_minrtt = 0xfdacdadf
	};
	static const struct tcp_dctcp_info dctcp = {
		.dctcp_enabled = 0xfdac,
		.dctcp_ce_state = 0xfadc,
		.dctcp_alpha = 0xbdabcada,
		.dctcp_ab_ecn = 0xbadbfafb,
		.dctcp_ab_tot = 0xfdacdadf
	};
	static const struct tcp_bbr_info bbr = {
		.bbr_bw_lo = 0xfdacdadf,
		.bbr_bw_hi = 0xfadcacdb,
		.bbr_min_rtt = 0xbdabcada,
		.bbr_pacing_gain = 0xbadbfafb,
		.bbr_cwnd_gain = 0xfdacdadf
	};
	static const uint32_t mem[] = { 0xaffacbad, 0xffadbcab };
	static uint32_t bigmem[SK_MEMINFO_VARS + 1];
	static const uint32_t mark = 0xabdfadca;

	const int fd = create_nl_socket(NETLINK_SOCK_DIAG);
	const unsigned int hdrlen = sizeof(struct inet_diag_msg);
	void *const nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
					 NLA_HDRLEN +
					 MAX(sizeof(bigmem), DEFAULT_STRLEN));

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	TEST_NLATTR_OBJECT(fd, nlh0, hdrlen,
			   init_inet_diag_msg, print_inet_diag_msg,
			   INET_DIAG_MEMINFO, pattern, minfo,
			   printf("{");
			   PRINT_FIELD_U(minfo, idiag_rmem);
			   printf(", ");
			   PRINT_FIELD_U(minfo, idiag_wmem);
			   printf(", ");
			   PRINT_FIELD_U(minfo, idiag_fmem);
			   printf(", ");
			   PRINT_FIELD_U(minfo, idiag_tmem);
			   printf("}"));

	TEST_NLATTR_OBJECT(fd, nlh0, hdrlen,
			   init_inet_diag_msg, print_inet_diag_msg,
			   INET_DIAG_VEGASINFO, pattern, vegas,
			   printf("{");
			   PRINT_FIELD_U(vegas, tcpv_enabled);
			   printf(", ");
			   PRINT_FIELD_U(vegas, tcpv_rttcnt);
			   printf(", ");
			   PRINT_FIELD_U(vegas, tcpv_rtt);
			   printf(", ");
			   PRINT_FIELD_U(vegas, tcpv_minrtt);
			   printf("}"));


	TEST_NLATTR_OBJECT(fd, nlh0, hdrlen,
			   init_inet_diag_msg, print_inet_diag_msg,
			   INET_DIAG_DCTCPINFO, pattern, dctcp,
			   printf("{");
			   PRINT_FIELD_U(dctcp, dctcp_enabled);
			   printf(", ");
			   PRINT_FIELD_U(dctcp, dctcp_ce_state);
			   printf(", ");
			   PRINT_FIELD_U(dctcp, dctcp_alpha);
			   printf(", ");
			   PRINT_FIELD_U(dctcp, dctcp_ab_ecn);
			   printf(", ");
			   PRINT_FIELD_U(dctcp, dctcp_ab_tot);
			   printf("}"));

	TEST_NLATTR_OBJECT(fd, nlh0, hdrlen,
			   init_inet_diag_msg, print_inet_diag_msg,
			   INET_DIAG_BBRINFO, pattern, bbr,
			   printf("{");
			   PRINT_FIELD_X(bbr, bbr_bw_lo);
			   printf(", ");
			   PRINT_FIELD_X(bbr, bbr_bw_hi);
			   printf(", ");
			   PRINT_FIELD_U(bbr, bbr_min_rtt);
			   printf(", ");
			   PRINT_FIELD_U(bbr, bbr_pacing_gain);
			   printf(", ");
			   PRINT_FIELD_U(bbr, bbr_cwnd_gain);
			   printf("}"));

	TEST_NLATTR_ARRAY(fd, nlh0, hdrlen,
			  init_inet_diag_msg, print_inet_diag_msg,
			  INET_DIAG_SKMEMINFO, pattern, mem, print_uint);

	memcpy(bigmem, pattern, sizeof(bigmem));

	TEST_NLATTR_ARRAY(fd, nlh0, hdrlen,
			  init_inet_diag_msg, print_inet_diag_msg,
			  INET_DIAG_SKMEMINFO, pattern, bigmem, print_uint);

	TEST_NLATTR_OBJECT(fd, nlh0, hdrlen,
			   init_inet_diag_msg, print_inet_diag_msg,
			   INET_DIAG_MARK, pattern, mark,
			   printf("%u", mark));

	TEST_NLATTR_OBJECT(fd, nlh0, hdrlen,
			   init_inet_diag_msg, print_inet_diag_msg,
			   INET_DIAG_CLASS_ID, pattern, mark,
			   printf("%u", mark));

	static const struct strval8 shutdown_vecs[] = {
		{ ARG_STR(0) },
		{ 1, "0x1 /* RCV_SHUTDOWN */" },
		{ 2, "0x2 /* SEND_SHUTDOWN */" },
		{ 3, "0x3 /* RCV_SHUTDOWN|SEND_SHUTDOWN */" },
		{ 4, "0x4 /* ???_SHUTDOWN */" },
		{ 23, "0x17 /* RCV_SHUTDOWN|SEND_SHUTDOWN|0x14 */" },
		{ 252, "0xfc /* ???_SHUTDOWN */" },
	};
	TAIL_ALLOC_OBJECT_CONST_PTR(uint8_t, shutdown);
	for (size_t i = 0; i < ARRAY_SIZE(shutdown_vecs); i++) {
		*shutdown = shutdown_vecs[i].val;
		TEST_NLATTR(fd, nlh0, hdrlen,
			    init_inet_diag_msg, print_inet_diag_msg,
			    INET_DIAG_SHUTDOWN,
			    sizeof(*shutdown), shutdown, sizeof(*shutdown),
			    printf("%s", shutdown_vecs[i].str));
	}

	char *const str = tail_alloc(DEFAULT_STRLEN);
	fill_memory_ex(str, DEFAULT_STRLEN, '0', 10);
	TEST_NLATTR(fd, nlh0, hdrlen,
		    init_inet_diag_msg, print_inet_diag_msg, INET_DIAG_CONG,
		    DEFAULT_STRLEN, str, DEFAULT_STRLEN,
		    printf("\"%.*s\"...", DEFAULT_STRLEN, str));
	str[DEFAULT_STRLEN - 1] = '\0';
	TEST_NLATTR(fd, nlh0, hdrlen,
		    init_inet_diag_msg, print_inet_diag_msg, INET_DIAG_CONG,
		    DEFAULT_STRLEN, str, DEFAULT_STRLEN,
		    printf("\"%s\"", str));

	/* INET_DIAG_MD5SIG */
	struct tcp_diag_md5sig md5s_arr[ARRAY_SIZE(md5sig_vecs)];

	for (size_t i = 0; i < ARRAY_SIZE(md5sig_vecs); i++) {
		memcpy(md5s_arr + i, &md5sig_vecs[i].val, sizeof(md5s_arr[0]));

		TEST_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_inet_diag_msg, print_inet_diag_msg,
				   INET_DIAG_MD5SIG, pattern,
				   md5sig_vecs[i].val,
				   printf("[%s]", md5sig_vecs[i].str));
	}

	TEST_NLATTR_ARRAY(fd, nlh0, hdrlen,
			  init_inet_diag_msg, print_inet_diag_msg,
			  INET_DIAG_MD5SIG, pattern, md5s_arr, print_md5sig);

	puts("+++ exited with 0 +++");
	return 0;
}
