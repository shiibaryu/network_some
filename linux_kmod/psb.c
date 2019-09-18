#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/smp.h>
#include <linux/err.h>
#include <linux/rtnetlink.h>
#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/pid.h>
#include <linux/if_ether.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/socket.h>
#include <uapi/linux/ip.h>
#include <net/genetlink.h>
#include <net/netns/generic.h>
#include <net/net_namespace.h>
#include <net/sock.h>

MODULE_AUTHOR("siiba@sfc.wide.ad.jp");
MODULE_DESCRIPTION("protocol stack bypass");
MODULE_LICENSE("GPL");

//キャラクタデバイスを作る
//ユーザーランドからの書き込みを取って、出力する

#define DEV_NAME  "psb/siiba"
#define PSB_VERSION	"0.0.0"

struct psb_dev{
        struct net_device *dev;
        char psb_path[IFNAMSIZ + 10];
        struct miscdevice mdev;
        pid_t pid;
};

static unsigned int psb_net_id;

struct psb_net{
        struct list_head dev_list;
};

static struct file_operations psb_fps = {
        .owner          = THIS_MODULE,
        .open           = psb_open,
        .read           = psb_read,
        .read_iter      = psb_read_iter,
        .write          = psb_write,
        .write          = write_iter,
        .poll           = psb_poll,
        .release        = psb_release,
};

static int psb_open()
{
        return 0;
}

static int psb_read()
{
        return 0;
}

static int psb_read_iter()
{     
        return 0;
}

static int psb_write()
{
        return 0;
}

static int write_iter()
{
        return 0;
}

static int psb_poll()
{
        return 0;
}

static int psb_release()
{
        return 0;
}

static int 
int init_psb_dev(struct psb_dev *psdev,struct net_device *dev)
{
        printk("insmod psb mmodule",PSB_VERSION);
        int ret;
        snprintf(psdev->path,IFNAMZIE + 10,"%s",DEV_NAME);
        psdev->mdev.minor = MISC_DYNAMIC_MINOR;
        psdev->mdev.fops  = &psb_fops;
        psdev->mdev.name  = psdev->path;
        psdev->dev        = dev;

        ret = misc_register(&psdev->mdev);
        if(ret<0){
                debug("failed to register mis device %s\n",psdev->path);
                goto misc_dev_failed;
        }

        return 0;
}

static struct pernet_operations psb_net_ops = {
        .init           = psb_init_net,
        .exit           = psb_exit_net,
        .id             = &psb_net_id,
        .size           = sizeof(struct psb_net),
};

static const struct proto_ops psb_proto_ops = {
        .family         = PF_PSB,
        .owner          = THIS_MODULE,
        .release        = psb_sock_release,
        .bind           = psb_sock_bind,
        .poll           = psb_sock_poll,
        .sendmsg        = psb_sendmsg,
        .recvmsg        = psb_recvmsg,
        .mmap           = sock_no_mmap,
};

static struct proto psb_proto = {
        .name           = "PSB",
        .owner          = THIS_MODULE,
        .obj_size       = sizeof(struct psb_sock),
};

static __net_init int psb_init_net(struct net *net)
{
        int ret;
        struct psb_dev *ps_dev;
        struct psb_net *psb_net = (struct psb_net *)net_generic(net,psb_net_id);

        INIT_LIST_HEAD(&psb_net->dev_list);
        
        for_each_netdev_rcu(net,dev){
                rtnl_lock();
                if (netdev_is_rx_handler_busy(dev)) {
			pr_info("dev %s rx_handler is already used. "
				"so, not registered to hpio\n", dev->name);
			rtnl_unlock();
			continue;
		}
                rtnl_unlock();
                
                ps_dev = kmalloc(sizeof(struct ps_dev),GFP_KERNEL);
                if(!ps_dev){
                        return -ENOMEM;
                }

                ret = init_psb_dev(ps_dev,dev);
                if(ret < 0){
                        pr_err("failed to register %s to hpio\n", dev->name);
			goto failed;
                }
                hpio_add_dev(hpnet, hpdev);
        }
failed:
        return ret;
}

static __net_exit int psb_exit_net(struct net *net)
{
        struct psb_net *psb_net = (struct psb_net *)net_generic(net,psb_net_id);

        list_for_each_safe
}
static struct pernet_operations psb_net_ops = {
        .init           = psb_init_net,
        .exit           = psb_exit_net,
        .id             = &psb_net_id,
        .size           = sizeof(struct psb_net),
};

static int __init psb_init_module(void)
{
        int ret;

        ret = register_pernet_subsys(&psb_net_ops);
        if(ret != 0){
                pr_err("proto_register failed %d\n",ret);
                goto pernet_register_failed;
        }

        ret = proto_register(&psb_proto,1);
        if(ret){
                pr_err("proto_register failed %d\n",rc);
                goto proto_register_failed;
        }

        ret = socket_register(&psb_family_ops);
        if(ret){
                pr_err("sock_register_failed %d\n",ret);
                goto sock_register_failed;
        }
        
        return 0;

sock_register_failed:
        proto_unregister(&psb_proto);
proto_register_failed:
        unregister_pernet_subsys(&psb_net_ops);
pernet_register_failed:
        return ret;
}
module_init(psb_init_moudule);

static void __exit psb_init_module(void)
{        
        printk("unload psb module",PSB_VERSION);

        sock_unregister(PF_PSB);
        proto_unregister(&psb_proto);
        unregister_pernet_subsys(&psb_net_ops);
}
module_exit(psb_exit_module);
