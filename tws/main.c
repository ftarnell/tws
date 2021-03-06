/*
 * Copyright (c) 2010 River Tarnell.  All rights reserved.
 * Use is subject to license terms.
 */

#include	<sys/types.h>
#include	<sys/resource.h>
#include	<stdio.h>
#include	<pwd.h>
#include	<grp.h>
#include	<unistd.h>
#include	<string.h>
#include	<errno.h>

#include	"config.h"
#include	"net.h"
#include	"log.h"
#include	"setup.h"

static int daemonise(int);
static void usage(void);

static void
usage() 
{
	fprintf(stderr,
"Usage: tws [-fhv] [-c <config>]\n"
"\n"
"    -h             Display this text\n"
"    -f             Run in foreground\n"
"    -c <config>    Use a different configuration file\n"
"                   (default: %s)\n"
"    -v             Display software version and exit\n"
, DEFAULT_CONFIG);
}

int
main(argc, argv)
	int	argc;
	char	**argv;
{
int		 fflag = 0;
const char	*config = DEFAULT_CONFIG;
int		 c;
int		 i;

	(void) argc;
	(void) argv;

	server_version = g_strdup_printf("Toolserver-Web-Server/%s",
			PACKAGE_VERSION);

	net_init();

	while ((c = getopt(argc, argv, "c:fhv")) != -1) {
		switch (c) {
		case 'c':
			config = optarg;
			break;

		case 'f':
			fflag = 1;
			break;

		case 'h':
			usage();
			return 0;

		case 'v':
			printf("%s\n", server_version);
			printf("Using libevent %s, %s, zlib %s, Glib %d.%d.%d\n",
				event_get_version(),
				SSLeay_version(SSLEAY_VERSION),
				zlibVersion(),
				glib_major_version,
				glib_minor_version,
				glib_micro_version);
			return 0;

		default:
			usage();
			return 1;
		}
	}

	if ((curconf = config_load(DEFAULT_CONFIG)) == NULL) {
		(void) fprintf(stderr, "cannot load configuration\n");
		return 1;
	}

	if (log_open() == -1) {
		(void) fprintf(stderr, "cannot open log file\n");
		return 1;
	}

	if (curconf->nfiles) {
	struct rlimit	lim;
		lim.rlim_cur = lim.rlim_max = curconf->nfiles;
		if (setrlimit(RLIMIT_NOFILE, &lim) == -1)
			log_warn("Cannot set nfiles: %d: %s",
				curconf->nfiles, strerror(errno));
	}

	log_notice("TWS/%s starting up", PACKAGE_VERSION);

	/* Set up listeners before setuid, since we need to be root to
	 * bind to port 80.
	 */
	if (net_listen() == -1) {
		log_error("Cannot set up network listeners");
		return 1;
	}

	if (daemonise(fflag) == -1)
		return 1;

	net_run();

	log_notice("TWS/%s shutting down", PACKAGE_VERSION);
	return 0;
}

static int
daemonise(fg)
	int	fg;
{
uid_t	uid;
gid_t	gid;
int	suid = 0, sgid = 0;

	(void) fg;

	if (chdir("/") == -1) {
		log_error("chdir(/): %s", strerror(errno));
		return -1;
	}

	if (curconf->user) {
	struct passwd	*pwd;
		if ((pwd = getpwnam(curconf->user)) == NULL) {
			log_error("Invalid username \"%s\"", curconf->user);
			return -1;
		}
		uid = pwd->pw_uid;
		gid = pwd->pw_gid;
		suid = sgid = 1;
	}

	if (curconf->group) {
	struct group	*grp;
		if ((grp = getgrnam(curconf->group)) == NULL) {
			log_error("Invalid group name \"%s\"", curconf->group);
			return -1;
		}
		gid = grp->gr_gid;
		sgid = 1;
	}

	if (suid && setgid(gid) == -1) {
		log_error("setgid(%s): %s", curconf->group, strerror(errno));
		return -1;
	}

	if (sgid && setuid(uid) == -1) {
		log_error("setuid(%s): %s", curconf->user, strerror(errno));
		return -1;
	}

	return 0;
}
