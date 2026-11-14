#!/bin/sh
set -eu
here=$(CDPATH= cd "$(dirname "$0")" && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT HUP INT TERM
flags="-std=c11 -O2 -g -DLAB_HOST_TEST -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast"
${HOSTCC:-cc} $flags -I "$here/../.." "$here/backend_test.c" "$here/../source/lab_buf.c" "$here/../source/lab_qdma.c" "$here/../source/lab_host.c" -o "$out/backend-test"
"$out/backend-test"
${HOSTCC:-cc} $flags -I "$here/../.." -DNPU_VERSION_PATCH=24 "$here/start_test.c" "$here/../source/lab_control.c" -o "$out/start-test"
"$out/start-test"
# Same real NDBG definitions with the historical and relocated base.
for mode in old new; do
 extra=
 [ "$mode" = old ] || extra=-DLAB_EN7523
 ${HOSTCC:-cc} -std=gnu11 -O2 -DEN7523 -DMT7915 $extra -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -I "$here/../.." "$here/debug_alias_test.c" -o "$out/debug-$mode"
 "$out/debug-$mode"
done
${HOSTCC:-cc} -std=gnu11 -O2 -ffunction-sections -fdata-sections -DEN7523 -DMT7915 -DLAB_EN7523 -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -I "$here/../.." "$here/forward_policy_test.c" -Wl,--gc-sections -o "$out/forward-policy"
"$out/forward-policy"
${HOSTCC:-cc} -std=c11 -O2 -Wall -Wextra "$here/mt76_consumer_test.c" -o "$out/mt76-consumer"
"$out/mt76-consumer"

${HOSTCC:-cc} $flags -DLAB_PROFILE=1 -I "$here/../.." "$here/backend_test.c" "$here/../source/lab_buf.c" "$here/../source/lab_qdma.c" "$here/../source/lab_host.c" -o "$out/backend-profile"
"$out/backend-profile"
${HOSTCC:-cc} $flags -DLAB_PROFILE=1 -Wall -Wextra -Wno-unused-variable -I "$here/../.." "$here/profile_test.c" "$here/../source/lab_profile.c" -o "$out/profile-test"
"$out/profile-test"

${HOSTCC:-cc} $flags -Wall -Wextra -Wno-unused-variable -I "$here/../.." "$here/rx_schedule_test.c" "$here/../source/lab_rx_schedule.c" -o "$out/rx-schedule"
"$out/rx-schedule"
