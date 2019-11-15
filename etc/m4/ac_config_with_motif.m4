dnl -*- Autoconf -*-

AC_DEFUN([AC_CONFIG_WITH_MOTIF],[

AC_MSG_CHECKING([for Motif library])

AS_VAR_PUSHDEF([ac_path_Var],[ac_motif_path])
AS_VAR_PUSHDEF([ac_incdir_Var],[ac_motif_incdir])
AS_VAR_PUSHDEF([ac_cppflags_Var],[ac_motif_cppflags])
AS_VAR_PUSHDEF([ac_libdir_Var],[ac_motif_libdir])
AS_VAR_PUSHDEF([ac_ldflags_Var],[ac_motif_ldflags])

dnl
dnl Only one needed if the package tree has a normal structure
dnl
AC_ARG_WITH([motif],
    [AC_HELP_STRING([--with-motif],[Motif installation directory])],
    [ac_path_Var=${withval}],
    [])

dnl
dnl Override the include directory
dnl
AC_ARG_WITH([motif-incdir],
    [AC_HELP_STRING([--with-motif-incdir],[Motif include directory])],
    [ac_incdir_Var=${withval}],
    [])

if test "x${ac_incdir_Var}" = x; then
  if test "x${ac_path_Var}" != x; then
    ac_cppflags_Var="-I${ac_path_Var}/include"
  fi
else
  ac_cppflags_Var="-I${ac_incdir_Var}"
fi

AC_SUBST(MOTIF_CPPFLAGS,[$ac_cppflags_Var])

AC_ARG_WITH([motif-libdir],
    [AC_HELP_STRING([--with-motif-libdir],[Motif library directory])],
    [ac_libdir_Var=${withval}],
    [])

if test "x${ac_libdir_Var}" = x; then
  if test "x${ac_path_Var}" != x; then
    ac_ldflags_Var="-L${ac_path_Var}/lib"
  fi
else
  ac_ldflags_Var="-L${ac_libdir_Var}"
fi

AC_SUBST(MOTIF_LDFLAGS,[$ac_ldflags_Var])
AC_SUBST(MOTIF_LIBS,[-lXm])

AC_MSG_RESULT([done])

AS_VAR_POPDEF([ac_path_Var])
AS_VAR_POPDEF([ac_incdir_Var])
AS_VAR_POPDEF([ac_cppflags_Var])
AS_VAR_POPDEF([ac_libdir_Var])
AS_VAR_POPDEF([ac_ldflags_Var])

])
