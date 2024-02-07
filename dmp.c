#include <linux/device-mapper.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/bio.h>
#include <linux/sysfs.h>

#define DM_MSG_PREFIX "dmp"

struct dmp_c {
    struct dm_dev *dev;
    uint64_t count_of_read;
    uint64_t count_of_write;
    uint64_t read_size;
    uint64_t write_size;
};

static struct dmp_c *dt;

/********************Utill functions********************/
static ssize_t get_avg_size_of_read(struct dmp_c *dt) 
{
    if (dt == NULL) {
        return -1;
    }

    if (dt->count_of_read == 0) {
        return 0;
    }

    return dt->read_size / dt->count_of_read;
}

static ssize_t get_avg_size_of_write(struct dmp_c *dt) {
    if (dt == NULL) {
        return -1;
    }

    if (dt->count_of_write == 0) {
        return 0;
    }

    return dt->write_size / dt->count_of_write;
}

static ssize_t get_avg_size(struct dmp_c *dt) {
    if (dt == NULL) {
        return -1;
    }

    if (dt->count_of_read + dt->count_of_write == 0) {
        return 0;
    }

    return (dt->read_size + dt->write_size) / (dt->count_of_read + dt->count_of_write);
}

static void update_stats(struct bio *bio) 
{
    unsigned int bio_size = bio->bi_iter.bi_size + bio->bi_iter.bi_bvec_done;

    switch (bio_op(bio)) {
        case REQ_OP_READ:
            dt->count_of_read++;
            dt->read_size += bio_size;
            break;
        case REQ_OP_WRITE:
            dt->count_of_write++;
            dt->write_size += bio_size; 
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
static struct kobject *stats_dir;

static ssize_t dmp_c_show(struct kobject *kobj, 
                        struct kobj_attribute *attr, char *buf) 
{
    if (dt == NULL) {
        return -1;
    }

    return sprintf(
        buf,
        "read:\n"
        "   regs: %llu\n"
        "   avg size: %lli\n"
        "write:\n"
        "   regs: %llu\n"
        "   avg size: %lli\n"
        "total:\n"
        "   regs: %llu\n"
        "   avg size: %lli\n",
        dt->count_of_read, get_avg_size_of_read(dt),
        dt->count_of_write, get_avg_size_of_write(dt),
        dt->count_of_read + dt->count_of_write, get_avg_size(dt));
}

static struct kobj_attribute dmp_attr = __ATTR(volumes, 0444, dmp_c_show, NULL);

int __init dm_dmp_init(void)
{
	int r = dm_register_target(&dmp_target);

	if (r < 0)
		DMERR("register failed %d\n", r);

    stats_dir = kobject_create_and_add("stats", &THIS_MODULE->mkobj.kobj);

    if (!stats_dir)
        return -ENOMEM;
    
    r = sysfs_create_file(stats_dir, &dmp_attr.attr);
    if (r < 0) 
        DMERR("failed to create the volumes file"
              "in /sys/modules/stats/volumes\n");

    return r;
}

void dm_dmp_exit(void)
{
    kobject_put(stats_dir);
	dm_unregister_target(&dmp_target);
}

module_init(dm_dmp_init);
module_exit(dm_dmp_exit);

MODULE_AUTHOR("Aytal Bagardynov <aytalbagardynov@mail.ru>");
MODULE_LICENSE("GPL");