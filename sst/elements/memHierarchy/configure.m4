dnl -*- Autoconf -*-
dnl vim:ft=config
dnl

AC_DEFUN([SST_memHierarchy_CONFIG], [
	mh_happy="yes"

  # Use global DRAMSim check
  SST_CHECK_DRAMSIM([],[],[AC_MSG_ERROR([DRAMSim requested but could not be found])])

  # Use global DRAMSim check
  SST_CHECK_HYBRIDSIM([],[],[AC_MSG_ERROR([HybridSim requested but could not be found])])

  AS_IF([test "$mh_happy" = "yes"], [$1], [$2])
])
