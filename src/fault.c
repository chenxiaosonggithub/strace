#include "defs.h"
#include "fault.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

int inject_fault(int pid, int nth)
{
	char buf[128];
	int fd;

	sprintf(buf, "/proc/%d/task/%d/fail-nth", pid, pid);
	fd = open(buf, O_RDWR);
	if (fd < 0) {
		error_msg("failed to open %s\n", buf);
		return -1;
	}

	sprintf(buf, "%d", nth);
	if (write(fd, buf, strlen(buf)) == -1) {
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

int fault_injected(int pid)
{
	char buf[128];
	int fd, ret;

	sprintf(buf, "/proc/%d/task/%d/fail-nth", pid, pid);
	fd = open(buf, O_RDWR);
	if (fd < 0) {
		error_msg("failed to open %s\n", buf);
		return -1;
	}

	ret = read(fd, buf, sizeof(buf) - 1);
	if (ret <= 0) {
		error_msg("failed to read fail-nth\n");
		goto err_out;
	}

	ret = atoi(buf);
	buf[0] = '0';
	if (write(fd, buf, 1) != 1) {
		error_msg("failed to write fail-nth\n");
		goto err_out;
	}

	return ret;

err_out:
	close(fd);
	return -1;
}
