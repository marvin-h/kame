# New ports collection makefile for:    apache HTTPD
# Version required:     1.3*
# Date created:         Fri Aug 25 16:42:36 CDT 1995
# Whom:                 ache@freebsd.org
#
# $FreeBSD: ports/www/apache13/Makefile,v 1.77 2000/02/02 21:14:03 ache Exp $
#

DISTNAME=       apache_1.3.14
PKGNAME=        apache-1.3.14
CATEGORIES=	www
MASTER_SITES=   http://www.apache.org/dist/ \
		ftp://ftp.ccs.neu.edu/net/mirrors/ftp.apache.org/apache/dist/ \
		ftp://ftp.rge.com/pub/infosystems/apache/dist/ \
		ftp://apache.compuex.com/pub/apache/dist/ \
		ftp://apache.arctic.org/pub/apache/dist/ \
		ftp://ftp.epix.net/pub/apache/dist/ \
		ftp://ftp.ameth.org/pub/mirrors/ftp.apache.org/apache/dist/ \
		ftp://ftp.connectnet.com/pub/www/apache/ \
		ftp://apache.technomancer.com/mirrors/apache/dist/ \
		ftp://ftp.raver.net/pub/ftp.apache.org/ \
		ftp://www3.service.digital.com/apache/dist/ \
		ftp://galileo.galilei.com/pub/apache/ \
		ftp://ftp.mtnranch.net/pub/apache/dist/ \
		ftp://ftp.iodynamics.com/pub/mirror/apache/dist/ \
		ftp://apache.nextpath.com/pub/apache/dist/

PATCH_SITES=	ftp://ftp.kame.net/pub/kame/misc/
PATCHFILES=	apache-1.3.14-v6-20001114.diff.gz
PATCH_DIST_STRIP=	-p1

MAINTAINER=	itojun@kame.net
Y2K=		http://www.apache.org/docs/misc/FAQ.html#year2000

#
# Set APACHE_PERF_TUNING env. variable to YES to get maximum performance
#

HAS_CONFIGURE=  yes
CONFIGURE_SCRIPT=	configure.v6
CONFIGURE_ARGS= \
		 --prefix=${PREFIX} \
		 --server-gid=nogroup \
		 --with-perl=${PERL} \
		 --with-layout=${FILESDIR}/FreeBSD.layout:FreeBSD \
		 --suexec-docroot=${PREFIX}/www/data \
		 --without-confadjust \
		 --enable-shared=remain \
		 --enable-module=most \
		 --enable-module=auth_db \
		 --disable-module=auth_dbm

OPTIM=-DHARD_SERVER_LIMIT=512 \
-DDOCUMENT_LOCATION=\\"${PREFIX}/www/data/\\" \
-DDEFAULT_PATH=\\"/bin:/usr/bin:${PREFIX}/bin\\"

.if defined(APACHE_PERF_TUNING) && ${APACHE_PERF_TUNING} == YES
OPTIM+= -DBUFFERED_LOGS
CFLAGS+= -O6 -fomit-frame-pointer
.endif

CONFIGURE_ENV= OPTIM='${OPTIM}'

MAN1= dbmmanage.1 htdigest.1 htpasswd.1
MAN8= ab.8 apachectl.8 apxs.8 httpd.8 logresolve.8 rotatelogs.8

pre-configure:
	chmod 755 ${WRKSRC}/${CONFIGURE_SCRIPT}

post-install:
	@if [ ! -f ${PREFIX}/etc/rc.d/apache.sh ]; then \
		${ECHO} "Installing ${PREFIX}/etc/rc.d/apache.sh startup file."; \
		${ECHO} "#!/bin/sh" > ${PREFIX}/etc/rc.d/apache.sh; \
		${ECHO} "[ -x ${PREFIX}/sbin/apachectl ] && ${PREFIX}/sbin/apachectl start > /dev/null && ${ECHO} -n ' apache'" >> ${PREFIX}/etc/rc.d/apache.sh; \
		chmod 751 ${PREFIX}/etc/rc.d/apache.sh; \
	fi

.include <bsd.port.mk>
