#include <stdio.h>
#define NETMAP_WITH_LIBS
#include <net/netmap_user.h>
#include <sys/poll.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

void nm_tx(struct nm_desc *nmd,char *buf,uint64_t num_pkt)
{
        struct netmap_ring *tx_ring = NETMAP_TXRING(nmd->nifp,0);
        uint32_t cur = tx_ring->cur;
        uint32_t space = 0;
        if(nm_ring_space(tx_ring) < num_pkt){
                printf("Availabe tx ring space is %d\n",nm_ring_space(tx_ring));
                printf("Number of packets is %d\n",num_pkt);
                space = nm_ring_space(tx_ring);
        }else{
                space = num_pkt;
        }

        for(;space>0;space--){
                struct netmap_slot *nm_slot = &tx_ring->slot[cur];
                char *tx_buf = NETMAP_BUF(tx_ring,nm_slot->buf_idx);
                nm_pkt_copy(buf,tx_buf,15);
                slot->len = 15;
                cur = nm_ring_next(tx_ring,cur);
        }

        tx_ring->head = tx_ring->cur = cur;
        ioctl(nmd->fd,NIOCTXSYNC,NULL);
}

int main(int argc,char *argv)
{
        if(argv != 2){
                printf("Please input the interface name");
                return -1;
        }
        char *ifname = NULL;
        strcpy(ifname,argv[2]);

        struct nm_desc *nmd = nm_open(ifname,NULL,0,NULL);
        if(nmd == NULL){
                perror("failed to open nm_desc");
                return -1;
        }
        char buf[15] = "Hello,World!";
        while(1){
                nm_tx(nmd,&buf,1);
        }

        return 0;
}
