#!/bin/sh

update_osip --update 0 --image $1
update_osip --restore
