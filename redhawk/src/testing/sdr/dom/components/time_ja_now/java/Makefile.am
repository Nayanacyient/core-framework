## This file is protected by Copyright. Please refer to the COPYRIGHT file
# distributed with this source distribution.
# #
# # This file is part of REDHAWK core.
# #
# # REDHAWK core is free software: you can redistribute it and/or modify it under
# # the terms of the GNU Lesser General Public License as published by the Free
# # Software Foundation, either version 3 of the License, or (at your option) any
# # later version.
# #
# # REDHAWK core is distributed in the hope that it will be useful, but WITHOUT
# # ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# # FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
# # details.
# #
# # You should have received a copy of the GNU Lesser General Public License
# # along with this program.  If not, see http://www.gnu.org/licenses/.
# #
#
if HAVE_JAVASUPPORT

time_ja_now.jar:
	mkdir -p bin
	find ./src -name "*.java" > fileList.txt
	$(JAVAC) -cp $(OSSIE_CLASSPATH) -d bin @fileList.txt
	$(JAR) cf ./time_ja_now.jar -C bin .
	rm fileList.txt

clean-local:
	rm -rf bin 

time_ja_now_jar_SOURCES := $(shell find ./src -name "*.java")

ossieName = time_ja_now
noinst_PROGRAMS = time_ja_now.jar

else

all-local:
	@echo "Java support disabled - time_ja_now will not be compiled"
endif
