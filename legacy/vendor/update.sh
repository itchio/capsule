#!/bin/bash -e
if [ $# -ne 1 ]; then
  echo "Usage: $0 ~/Dev/shoom"
  exit 1
fi

DEP_PATH=$(readlink -f $1)
DEP_NAME=$(readlink -f ./$(basename ${DEP_PATH}))

if [ "${DEP_PATH}" == "${DEP_NAME}" ]; then
  echo "Use an absolute path different from ${DEP_NAME}"
  exit 1
fi

if [ ! -d "${DEP_PATH}" ]; then
  echo "${DEP_PATH} is not a directory"
  exit 1
fi

if [ ! -d "${DEP_NAME}" ]; then
  echo "${DEP_NAME} is not a known dependency (not a directory)"
  exit 1
fi

butler wipe "${DEP_NAME}"
butler ditto "${DEP_PATH}" "${DEP_NAME}"
(cd "${DEP_NAME}" && git rev-parse HEAD) > "${DEP_NAME}.version"
butler wipe "${DEP_NAME}/.git"

echo "Updated ${DEP_NAME} to $(cat "${DEP_NAME}.version")"

