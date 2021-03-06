#!/usr/bin/env sh

LINUX_SRC_DIR="/home/soramichi/src/linux-source-3.16"
CFLAGS="-Wbad-function-cast -Wdeclaration-after-statement -Wformat-security -Wformat-y2k -Winit-self -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wno-system-headers -Wold-style-definition -Wpacked -Wredundant-decls -Wshadow -Wstrict-aliasing=3 -Wstrict-prototypes -Wswitch-default -Wswitch-enum -Wundef -Wwrite-strings -Wformat -DHAVE_ARCH_X86_64_SUPPORT -DHAVE_PERF_REGS_SUPPORT -O6 -fno-omit-frame-pointer -ggdb3 -funwind-tables -Wall -Wextra -std=gnu99 -fstack-protector-all -D_FORTIFY_SOURCE=2 -I${LINUX_SRC_DIR}/tools/perf/util/include -I${LINUX_SRC_DIR}/tools/perf/arch/x86/include -I${LINUX_SRC_DIR}/tools/include/ -I${LINUX_SRC_DIR}/arch/x86/include/uapi -I${LINUX_SRC_DIR}/arch/x86/include -I${LINUX_SRC_DIR}/include/uapi -I${LINUX_SRC_DIR}/include -I${LINUX_SRC_DIR}/tools/perf/util -I${LINUX_SRC_DIR}/tools/perf -I${LINUX_SRC_DIR}/tools/lib/ -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DHAVE_GTK2_SUPPORT -DNO_LIBPERL -DHAVE_TIMERFD_SUPPORT -DNO_LIBPYTHON -DNO_DEMANGLE -DHAVE_BACKTRACE_SUPPORT -DHAVE_LIBNUMA_SUPPORT"
LIBS_PERF="${LINUX_SRC_DIR}/tools/perf/libperf.a ${LINUX_SRC_DIR}/tools/lib/api/libapikfs.a ${LINUX_SRC_DIR}/tools/lib/traceevent/libtraceevent.a"
LIBS_STD="-lm -lpthread -ldl -lelf -lunwind -lunwind-x86_64"

g++ -std=c++11 -shared -fPIC hash.cpp -o hash.o
gcc -L . hash.o $CFLAGS mat.c dummy_for_3.16.c $LIBS_PERF $LIBS_STD -o mat
