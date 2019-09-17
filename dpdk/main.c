/*
 * 手作りルーターサンプルプログラム (実装済みバージョン)
 *
 * Copyright (C) YAMAHA CORPORATION. All rights reserved.
 *
 * DPDK付属サンプルプログラム "l2fwd" を改造して作成
 * - l2fwdの著作権表示: Copyright(c) 2010-2016 Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>
#include <netinet/ip_icmp.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>


// 本プログラムで扱うNICポート数
#define NUM_PORTS 2


// 自身のMACアドレス
static struct rte_ether_addr my_eth_addr[NUM_PORTS];

// パケット転送先のMACアドレス (適切なアドレスに書き換えること)
static struct rte_ether_addr dest_eth_addr[NUM_PORTS] = {
	{ { 0x00, 0x0c, 0x29, 0x9d, 0x77, 0x8c } },	// port0 から送信する際の宛先
	{ { 0x00, 0x0c, 0x29, 0xe1, 0x2d, 0x7d } },	// port1 から送信する際の宛先
};

// 自身のIPv4アドレス (適切なアドレスに書き換えること)
union ipv4_addr {
	uint8_t octet[4];
	uint32_t be;
};
union ipv4_addr my_ipv4_addr[NUM_PORTS] = {
	{ { 192, 168, 11, 1 } },	// port0
	{ { 192, 168, 12, 1 } },	// port1
};

// パケットバッファ (mbuf) のプール
static struct rte_mempool *pkt_pool;

// プログラム実行中止指示
static volatile int force_quit = 0;


/*
 * ICMP Echo Requestに応答する (Echo Replyを送信する)
 *
 * pkt:      Echo Requestパケット
 * pkt_eth:  Echo RequestパケットのEthernetヘッダー部
 * pkt_ipv4: Echo RequestパケットのIPv4ヘッダー部
 * pkt_ipv4: Echo RequestパケットのICMPヘッダー部
 *
 * return: 成功時は0、失敗時は-1
 */
static int
reply_icmp_echo(struct rte_mbuf *pkt, struct rte_ether_hdr *pkt_eth, struct rte_ipv4_hdr *pkt_ipv4, struct rte_icmp_hdr *pkt_icmp)
{
	uint16_t tx_port;
	int ret;

	// 送信元ポートを選択する (Echo Requestの送信先アドレスが一致するポートを選択)
	for (tx_port = 0; tx_port < NUM_PORTS; tx_port++) {
		if (pkt_ipv4->dst_addr == my_ipv4_addr[tx_port].be) {
			break;
		}
	}
	if (tx_port == NUM_PORTS) {
		printf("no tx port found\n");
		return -1;
	}


	/*
	 * TODO: 以下を実装する必要あり
	 *
	 * - MACアドレスの送信元・送信先を入れ替える
	 * - IPv4 TTLを初期値 (例: 64) に戻し、送信元・送信先アドレスを入れ替える
	 * - IPv4チェックサムを更新する
	 * - ICMPメッセージ種別を Echo Reply に変更する
	 * - ICMPチェックサムを更新する
	 */

	rte_ether_addr_copy(&my_eth_addr[tx_port],&pkt_eth->s_addr);

	rte_ether_addr_copy(&dest_eth_addr[tx_port],&pkt_eth->d_addr);

	pkt_ipv4->time_to_live = 64;

	uint32_t ret_d = pkt_ipv4->dst_addr;
	pkt_ipv4->dst_addr = pkt_ipv4->src_addr;	
	pkt_ipv4->src_addr = ret_d;
	
	pkt_ipv4->hdr_checksum ^= rte_raw_cksum(pkt_ipv4,sizeof(struct rte_ipv4_hdr));
	if(pkt_ipv4->hdr_checksum == 0x0000){
		pkt_ipv4->hdr_checksum = 0xFFFF;
	}

	pkt_icmp->icmp_type = RTE_IP_ICMP_ECHO_REPLY;

	pkt_icmp->icmp_cksum ^= rte_raw_cksum(pkt_icmp,sizeof(struct rte_icmp_hdr));
	

	// パケットを送信する (送信キュー: 1)
	ret = rte_eth_tx_burst(tx_port, 1, &pkt, 1);
	if (ret != 1) {
		rte_exit(EXIT_FAILURE, "rte_eth_tx_burst = %d\n", ret);
	}

	return 0;
}

/*
 * IPv4パケットを転送する
 *
 * rx_port:  パケットを受信したポートの番号
 * pkt:      受信したパケット
 * pkt_eth:  受信したパケットのEthernetヘッダー部
 * pkt_ipv4: 受信したパケットのIPv4ヘッダー部
 *
 * return: 成功時は0、失敗時は-1
 */
static int
forward_ipv4_pkt(uint16_t rx_port, struct rte_mbuf *pkt, struct rte_ether_hdr *pkt_eth, struct rte_ipv4_hdr *pkt_ipv4)
{
	uint16_t tx_port;
	uint8_t ipv4_ttl;
	int ret;

	// 転送先のポートを決める (ポート0で受信したら1へ、1で受信したら0へ)
	tx_port = !rx_port;


	/*
	 * TODO: 以下を実装する必要あり
	 *
	 * - MACアドレスを書き換える
	 * - IPv4 TTL (Time to live) をデクリメントし、チェックサムを更新する
	 */
	//src書き換え
	rte_ether_addr_copy(&my_eth_addr[tx_port],&pkt_eth->s_addr);

	//dst書き換え
	rte_ether_addr_copy(&dest_eth_addr[tx_port],&pkt_eth->d_addr);
	
	//ipv4 ttlのディクリメント
	pkt_ipv4->time_to_live -= 1;

	//チェックサムの更新r
	pkt_ipv4->hdr_checksum = 0;
	pkt_ipv4->hdr_checksum = rte_ipv4_cksum(pkt_ipv4);

	
	// パケットを送信する (送信キュー: 0)
	ret = rte_eth_tx_burst(tx_port, 0, &pkt, 1);
	if (ret != 1) {
		rte_exit(EXIT_FAILURE, "rte_eth_tx_burst(%u) = %d\n", tx_port, ret);
	}

	return 0;
}

/*
 * 受信パケットをポーリングし、他ポートへの転送やPing応答を行う
 */
static void
do_polling(uint16_t rx_port)
{
	printf("entering polling thread (Rx port: %u)\n", rx_port);

	while (!force_quit) {
		struct rte_mbuf *pkt;
		struct rte_ether_hdr *pkt_eth;
		struct rte_ipv4_hdr *pkt_ipv4;
		struct rte_icmp_hdr *pkt_icmp;

		// パケットの受信を試みる
		// - 受信パケットが存在しない場合はループする
		if (rte_eth_rx_burst(rx_port, 0, &pkt, 1) == 0) {
			continue;
		}

		/* 以下、受信したパケットの処理 */

		// パケットのEthernetヘッダーを抽出する
		pkt_eth = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);

		printf("[port%u] recv: ether_type: 0x%04x, src: %02x:%02x:%02x:%02x:%02x:%02x, dest: %02x:%02x:%02x:%02x:%02x:%02x\n",
				rx_port,
				rte_be_to_cpu_16(pkt_eth->ether_type),
				pkt_eth->s_addr.addr_bytes[0],
				pkt_eth->s_addr.addr_bytes[1],
				pkt_eth->s_addr.addr_bytes[2],
				pkt_eth->s_addr.addr_bytes[3],
				pkt_eth->s_addr.addr_bytes[4],
				pkt_eth->s_addr.addr_bytes[5],
				pkt_eth->d_addr.addr_bytes[0],
				pkt_eth->d_addr.addr_bytes[1],
				pkt_eth->d_addr.addr_bytes[2],
				pkt_eth->d_addr.addr_bytes[3],
				pkt_eth->d_addr.addr_bytes[4],
				pkt_eth->d_addr.addr_bytes[5]);

		// Ether Type 別の処理
		switch (rte_be_to_cpu_16(pkt_eth->ether_type)) {
			case RTE_ETHER_TYPE_ARP:
				/* ARPパケットの処理 */
				break;

			case RTE_ETHER_TYPE_IPV4:
				/* IPv4パケットの処理 */

				pkt_ipv4 = (struct rte_ipv4_hdr *)(pkt_eth + 1);
				pkt_icmp = (struct rte_icmp_hdr *)(pkt_ipv4 + 1);

				// 自分宛のICMP Echo Requestに応答する
				if (pkt_ipv4->dst_addr == my_ipv4_addr[rx_port].be) {
					if (pkt_ipv4->next_proto_id != IPPROTO_ICMP) {
						printf("[port%u] drop non-supported IPv4 packet\n", rx_port);
						break;
					}
					if (pkt_icmp->icmp_type != ICMP_ECHO) {
						printf("[port%u] drop non-supported ICMP packet\n", rx_port);
						break;
					}
					if (reply_icmp_echo(pkt, pkt_eth, pkt_ipv4, pkt_icmp) == 0) {
						printf("[port%u] send ICMP reply\n", rx_port);
						continue;
					}
					break;
				}

				// 他方のポートへパケットを転送する
				if (forward_ipv4_pkt(rx_port, pkt, pkt_eth, pkt_ipv4) == 0) {
					printf("[port%u] forward packet\n", rx_port);
					continue;
				}
				break;

			default:
				break;
		}

		// 処理されなかったパケットを破棄する (プールに戻す)
		rte_pktmbuf_free(pkt);
	}
}

/*
 * ポーリングスレッドのエントリーポイント
 */
static int
thread_main(__attribute__((unused)) void *dummy)
{
	unsigned lcore = rte_lcore_id();

	if (lcore >= NUM_PORTS) {
		return -1;
	}

	do_polling(lcore);

	return 0;
}

/*
 * プログラム実行中止指示 (シグナル) を処理する
 */
static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n", signum);
		force_quit = 1;
	}
}

/*
 * プログラムのエントリーポイント
 * NICのポートを初期化し、ポーリングスレッドを起動する
 */
int
main(int argc, char **argv)
{
	uint16_t num_rx_descripter = 1024;
	uint16_t num_tx_descripter = 1024;
	static const size_t MEMPOOL_CACHE_SIZE = 256;
	unsigned int nb_mbufs;
	uint16_t port;
	unsigned lcore;
	int ret;

	// DPDKの動作基盤を初期化する (EAL: Environment Abstraction Layer)
	ret = rte_eal_init(argc, argv);
	if (ret < 0) {
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	}

	// プログラム実行中止指示 (Ctrl+C 等) を受け取れるようにする
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	// DPDKにアサインされたNICポート数を確認
	if (rte_eth_dev_count_avail() != NUM_PORTS) {
		rte_exit(EXIT_FAILURE, "%d Ethernet ports required - bye\n", NUM_PORTS);
	}

	// パケットバッファ (mbuf) のプールを作成する
	// - プールされたバッファはNICへ受信用に割り当てられ、パケット受信後にNICから本プログラムへ渡される
	// - 本プログラムがパケット送信のためにNICへバッファを渡し、NICが送信を完了すると (*) 、バッファがプールに戻される
	//   * 正確には送信済みバッファの数が、NICに設定されたバッファ解放閾値を超えたタイミングで戻される
	nb_mbufs = RTE_MAX(NUM_PORTS * (num_rx_descripter + (num_tx_descripter * 2) + 1 + MEMPOOL_CACHE_SIZE), 8192U);
	pkt_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
			MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, SOCKET_ID_ANY);
	if (pkt_pool == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
	}

	// ポートを初期化する
	for (port = 0; port < NUM_PORTS; port++) {
		struct rte_eth_conf port_conf = { 0 };
		uint16_t queue;

		printf("Initializing port %u... ", port);

		// 受信キュー x1, 送信キュー x2 でポートを使用する
		// - 送信キュー1: ポート間パケット転送用, キュー2: ICMP Echo Reply用
		ret = rte_eth_dev_configure(port, 1, 2, &port_conf);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n", ret, port);
		}

		// 設定可能なDMAディスクリプターの数を探る
		ret = rte_eth_dev_adjust_nb_rx_tx_desc(port, &num_rx_descripter, &num_tx_descripter);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE,
				 "Cannot adjust number of descriptors: err=%d, port=%u\n", ret, port);
		}

		// ポートのMACアドレスを取得する
		rte_eth_macaddr_get(port, &my_eth_addr[port]);

		// 受信キューにパケットバッファのプールを割り当てる
		ret = rte_eth_rx_queue_setup(port, 0, num_rx_descripter, SOCKET_ID_ANY, NULL, pkt_pool);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n", ret, port);
		}

		// 送信キューにパケットバッファのプールを割り当てる
		for (queue = 0; queue < 2; queue++) {
			ret = rte_eth_tx_queue_setup(port, queue, num_tx_descripter, SOCKET_ID_ANY, NULL);
			if (ret < 0) {
				rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup(%u):err=%d, port=%u\n", queue, ret, port);
			}
		}

		// ポートの使用を開始する
		ret = rte_eth_dev_start(port);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n", ret, port);
		}

		printf("port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
				port,
				my_eth_addr[port].addr_bytes[0],
				my_eth_addr[port].addr_bytes[1],
				my_eth_addr[port].addr_bytes[2],
				my_eth_addr[port].addr_bytes[3],
				my_eth_addr[port].addr_bytes[4],
				my_eth_addr[port].addr_bytes[5]);
	}

	ret = 0;

	// ポーリングスレッドを起動する (1ポート1スレッド)
	rte_eal_mp_remote_launch(thread_main, NULL, CALL_MASTER);

	// 全スレッドが終了するまで待機する
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		if (rte_eal_wait_lcore(lcore) < 0) {
			ret = -1;
			break;
		}
	}

	// ポートの使用を終了する
	RTE_ETH_FOREACH_DEV(port) {
		printf("Closing port %d...", port);
		rte_eth_dev_stop(port);
		rte_eth_dev_close(port);
		printf(" Done\n");
	}

	printf("Bye...\n");

	return ret;
}
