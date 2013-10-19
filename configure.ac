AC_INIT([mqttdisplay],[0.0.1],[michael@no-surprises.co.uk],[mqttdisplay],[https://github.com/mgdm/MQTTDisplay])
AM_INIT_AUTOMAKE([1.9 foreign])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_INSTALL
AC_FUNC_STRERROR_R

AC_DEFUN([REQUIRE_LIB], [ {
		AC_ARG_WITH([$1], AC_HELP_STRING([--with-$1=<path>],[Location where $4 is installed]),[],[with_$1=default])
		AS_IF( [test "x$with_$1" != xdefault],
		[
			 LDFLAGS="$LDFLAGS -L${with_$1}/lib"
			 CFLAGS="$CFLAGS -I${with_$1}/include"
		])

		AC_CHECK_LIB($2,$3,[],
		[
			AC_MSG_ERROR([$4 was not found, try specifying with --with-$1])
		])

} ] )


REQUIRE_LIB(libmosquitto,mosquitto,mosquitto_lib_init,[Mosquitto MQTT library])
REQUIRE_LIB(libsureelec,sureelec,libsureelec_create,[libsureelec display library])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
