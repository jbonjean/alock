#!/bin/sh
set -e

if ! ./alock; then
	echo "already locked"
	exit 1
fi

echo "lock acquired"
sleep 10
