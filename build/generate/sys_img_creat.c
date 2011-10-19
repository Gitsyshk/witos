#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/types.h>

#define DBG_STR_LEN 128
#define LEN (1024 * 8)
#define GB_SYSCFG_VER 7

#define GB_SYSCFG_MAGICNUM  0x5a5a5a5a

typedef __u32 u32;
typedef __u16 u16;

struct sys_config
{
	__u32 magic;
	__u32 size;
	__u32 offset;
	__u32 checksum;
};

static u16 calc_checksum(const void *buff, u32 size)
{
	int n;
	u32	chksum;
	const u16 *p;

	chksum = 0;

	for (n = size, p = buff; n > 0; n -= 2, p++)
		chksum += *p;

	chksum = (chksum & 0xffff) + (chksum >> 16);
	chksum = (chksum & 0xffff) + (chksum >> 16);

	return chksum & 0xffff;
}

static u32 checksum(struct sys_config *g_sys)
{
	g_sys->checksum = 0;
	g_sys->checksum = ~calc_checksum(g_sys, g_sys->offset + g_sys->size);
	g_sys->checksum |= GB_SYSCFG_VER << 16;

	return g_sys->checksum;
}

int main(int argc, char *argv[])
{
	struct sys_config *sys;
	const char *txt_file, *img_file;
	char str[DBG_STR_LEN];
	int txt_fd, img_fd;
	int ret;
	struct stat file_stat;
	char buff[LEN];
	char *data;

	if (argc != 3)
	{
		printf("Usage: %s sysconf.txt sysconf.img\n", argv[0]);
		return -EINVAL;
	}

	txt_file = argv[1];
	img_file = argv[2];

	txt_fd = open(txt_file, O_RDONLY);
	if (txt_fd < 0)
	{
		sprintf(str, "open %s", txt_file);
		perror(str);
		return txt_fd;
	}

	img_fd = open(img_file, O_WRONLY | O_CREAT, 0644);
	if (img_fd < 0)
	{
		sprintf(str, "open %s", img_file);
		perror(str);
		ret = img_fd;
		goto L1;
	}

	ret = fstat(txt_fd, &file_stat);
	if (ret < 0)
	{
		printf("Get file state failed!\n");
		goto L2;
	}

	sys = (struct sys_config *)buff;
	data = buff + sizeof(*sys);

	// init sysconfig head
	sys->magic  = GB_SYSCFG_MAGICNUM;
	sys->size   = file_stat.st_size;
	sys->offset = sizeof(*sys);

	// copy txt_file to img_file
	ret = read(txt_fd, data, LEN - sys->offset);
	if (ret < 0) {
		sprintf(str, "read %s", txt_file);
		perror(str);
		goto L2;
	}

	checksum(sys);

	printf("Create sysconfig.img success! The chk sum is 0x%08X\n", sys->checksum);

	ret = write(img_fd, buff, sys->offset + sys->size);
	if (ret < 0) {
		sprintf(str, "write %s", img_file);
		perror(str);
		goto L2;
	}

L2:
	close(img_fd);
L1:
	close(txt_fd);

	return ret;
}
