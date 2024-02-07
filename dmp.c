#include <linux/device-mapper.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/bio.h>
#include <linux/sysfs.h>

#define DM_MSG_PREFIX "dmp"

struct dmp_operation_stat {
    uint64_t reqs;
    uint64_t total_size;
};

struct dmp_c {
    struct dm_dev *dev;
    struct dmp_operation_stat read_op_stat;
    struct dmp_operation_stat write_op_stat;
};

static struct dmp_c *dt;

/********************Utill functions********************/
static uint64_t avg_size(uint64_t sum, uint64_t count) 
{
    if (count == 0) {
        return 0;
    }

    return sum / count;
}

static void update_stats(struct bio *bio) 
{
    unsigned int bio_size = bio->bi_iter.bi_size;

    switch (bio_op(bio)) {
        case REQ_OP_READ:
            dt->read_op_stat.reqs++;
            dt->read_op_stat.total_size += bio_size;
            break;
        case REQ_OP_WRITE:
            dt->write_op_stat.reqs++;
            dt->write_op_stat.total_size += bio_size;
            break;
        default:
            break;
    }
}

/*
 * Construct a proxy mapping: <dev_path>
 */
static int dmp_ctr(struct dm_target *ti, unsigned int argc, char **argv) 
{
    if (argc != 1) {
        ti->error = "Invalid argument count";
        return -EINVAL;
    }

    dt = kmalloc(sizeof(struct dmp_c), GFP_KERNEL);
    if (dt == NULL) {
        ti->error = "dm-dmp: Cannot allocate linear context";
        return -ENOMEM;
    }

    if (dm_get_device(ti, argv[0], dm_table_get_mode(ti->table), &dt->dev)) {
        ti->error = "Device lookup failed";
        kfree(dt);
        return -EINVAL;
    }

    return 0;
}

static int dmp_map(struct dm_target *ti, struct bio *bio) 
{    
    update_stats(bio);

    bio_set_flag(bio, DM_MAPIO_REMAPPED);
    bio_set_dev(bio, dt->dev->bdev);

    return DM_MAPIO_REMAPPED;
}

static void dmp_dtr(struct dm_target *ti) 
{
    dm_put_device(ti, dt->dev);
    kfree(dt);
}

static struct target_type dmp_target = {
  .name    = "dmp",
  .version = {1, 0, 0},
  .module  = THIS_MODULE,
  .ctr     = dmp_ctr,
  .map     = dmp_map,
  .dtr     = dmp_dtr,
};

/********************Sysfs functions********************/
static ssize_t dmp_c_show(struct kobject *kobj, 
                        struct kobj_attribute *attr, char *buf) 
{
    if (dt == NULL) {
        return -1;
    }

    struct dmp_operation_stat read_stat = dt->read_op_stat;
    struct dmp_operation_stat write_stat = dt->write_op_stat;

    return sprintf(
        buf,
        "read:\n"
        "   regs: %llu\n"
        "   avg size: %llu\n"
        "write:\n"
        "   regs: %llu\n"
        "   avg size: %llu\n"
        "total:\n"
        "   regs: %llu\n"
        "   avg size: %llu\n",
        read_stat.reqs, avg_size(read_stat.total_size, read_stat.reqs),
        write_stat.reqs, avg_size(write_stat.total_size, write_stat.reqs),
        read_stat.reqs + write_stat.reqs, avg_size(read_stat.total_size + write_stat.total_size, read_stat.reqs + write_stat.reqs));
}

static struct kobject *stat_kobj;

static struct kobj_attribute dmp_attr = __ATTR(volumes, 0444, dmp_c_show, NULL);

int __init dm_dmp_init(void)
{
	int r = dm_register_target(&dmp_target);

	if (r < 0)
		DMERR("register failed %d\n", r);

    stat_kobj = kobject_create_and_add("stat", &THIS_MODULE->mkobj.kobj);

    if (!stat_kobj)
        return -ENOMEM;
    
    r = sysfs_create_file(stat_kobj, &dmp_attr.attr);
    if (r < 0) 
        DMERR("failed to create the volumes file in /sys/modules/stat/volumes\n");

    return r;
}

void dm_dmp_exit(void)
{
    kobject_put(stat_kobj);
    sysfs_remove_file(stat_kobj, &dmp_attr.attr);
	dm_unregister_target(&dmp_target);
}

module_init(dm_dmp_init);
module_exit(dm_dmp_exit);

MODULE_AUTHOR("Aytal Bagardynov <aytalbagardynov@mail.ru>");
MODULE_LICENSE("GPL");