dnl -*- Autoconf -*-

AC_DEFUN([AC_CONFIG_WITH_PAPER],[

AC_MSG_CHECKING([for Paper library])

AS_VAR_PUSHDEF([ac_path_Var],[ac_paper_path])
AS_VAR_PUSHDEF([ac_incdir_Var],[ac_paper_incdir])
AS_VAR_PUSHDEF([ac_cppflags_Var],[ac_paper_cppflags])
AS_VAR_PUSHDEF([ac_libdir_Var],[ac_paper_libdir])
AS_VAR_PUSHDEF([ac_ldflags_Var],[ac_paper_ldflags])

dnl
dnl Only one needed if the package tree has a normal structure
dnl
AC_ARG_WITH([paper],
    [AC_HELP_STRING([--with-paper],[Paper installation directory])],
    [ac_path_Var=${withval}],
    [])

dnl
dnl Override the include directory
dnl
AC_ARG_WITH([paper-incdir],
    [AC_HELP_STRING([--with-paper-incdir],[Paper include directory])],
    [ac_incdir_Var=${withval}],
    [])

if test "x${ac_incdir_Var}" = x; then
  if test "x${ac_path_Var}" != x; then
    ac_cppflags_Var="-I${ac_path_Var}/include"
  fi
else
  ac_cppflags_Var="-I${ac_incdir_Var}"
fi

AC_SUBST(PAPER_CPPFLAGS,[$ac_cppflags_Var])

AC_ARG_WITH([paper-libdir],
    [AC_HELP_STRING([--with-paper-libdir],[Paper library directory])],
    [ac_libdir_Var=${withval}],
    [])

if test "x${ac_libdir_Var}" = x; then
  if test "x${ac_path_Var}" != x; then
    ac_ldflags_Var="-L${ac_path_Var}/lib"
  fi
else
  ac_ldflags_Var="-L${ac_libdir_Var}"
fi

AC_SUBST(PAPER_LDFLAGS,[$ac_ldflags_Var])
AC_SUBST(PAPER_LIBS,[-lXm])

AC_MSG_RESULT([done])

AS_VAR_POPDEF([ac_path_Var])
AS_VAR_POPDEF([ac_incdir_Var])
AS_VAR_POPDEF([ac_cppflags_Var])
AS_VAR_POPDEF([ac_libdir_Var])
AS_VAR_POPDEF([ac_ldflags_Var])

])
