# *********************************************************************************** *
# Copyright 2017 by Bodo Bauer <bb@bb-zone.com>
#
# This file is part of the RPI Sensor Client
#
# ReadDHT11 is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# MQTTActionD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with ReadDHT11.  If not, see <http://www.gnu.org/licenses/>.
# *********************************************************************************** *

INSTALL(PROGRAMS rpisensorclient      DESTINATION etc/init.d)
INSTALL(FILES rpisensorclient.cfg  DESTINATION etc)
