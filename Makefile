# Generated automatically from Makefile.in by configure.
# Makefile -- makefile for xacc
# Copyright (C) 1997 Robin Clark                                   
# Copyright (C) 1998 Rob Browning <rlb@cs.utexas.edu>
#                                                                  
# This program is free software; you can redistribute it and/or    
# modify it under the terms of the GNU General Public License as   
# published by the Free Software Foundation; either version 2 of   
# the License, or (at your option) any later version.              
#                                                                  
# This program is distributed in the hope that it will be useful,  
# but WITHOUT ANY WARRANTY; without even the implied warranty of   
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    
# GNU General Public License for more details.                     
#                                                                  
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software      
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.        
#                                                                  
#   Author: Robin Clark                                            
# Internet: rclark@rush.aero.org                                   
#  Address: 609 8th Street                                         
#           Huntington Beach, CA 92648-4632                        

srcdir  = .

PREFIX  = /usr/local
INSTALL = /usr/bin/ginstall -c
INSTALL_DATA = ${INSTALL} -m 644
TARGET  = xacc
DOCDIR	= share/xacc/Docs
CPU     = @target_cpu@

LIBS=-lpng -ljpeg -lz -lm

######################################################################
# Description of targets:
#
#   default      -- make the application
#   depend       -- generate the dependencies
#   clean        -- remove *.a, *.o, *.bak, and *~
#   distclean    -- get rid of config files too...
#   install      -- installs everything

default:
	@echo " "
	@echo "Please choose one of the following targets:"
	@echo "motif           dynamically linked motif version"
	@echo "motif-static    statically linked motif version"
	@echo "gnome           gnome/gtk version"
	@echo " "

motif:
	@cd lib;    $(MAKE) motif
	@cd src;    $(MAKE) motif

# link in motif libs statically
motif-static:
	@cd lib;    $(MAKE) motif
	@cd src;    $(MAKE) motif-static

gnome:
	@cd lib;    $(MAKE) gnome
	@cd src;    $(MAKE) gnome

depend:
	@cd lib;    $(MAKE) depend
	@cd src;    $(MAKE) depend

clean:
	rm -f *~ *.o *.bak
	@cd lib;    $(MAKE) clean
	@cd src;    $(MAKE) clean

distclean: clean
	rm -f *~ *.o *.bak Makefile xacc xacc.gtk.bin
	rm -f config.cache config.log config.status config.h
	@cd lib;    $(MAKE) distclean
	@cd src;    $(MAKE) distclean

tagsfiles := $(shell find -name "*.[ch]")

TAGS: ${tagsfiles}
	etags ${tagsfiles}    

install: $(TARGET)
	@mkdir -p $(PREFIX)/bin
	$(INSTALL) $(TARGET) $(PREFIX)/bin/$(TARGET)
	$(INSTALL) $(TARGET).bin $(PREFIX)/bin/$(TARGET).bin
	@mkdir -p $(PREFIX)/$(DOCDIR)
	$(INSTALL_DATA) Docs/*.html $(PREFIX)/$(DOCDIR)
	$(INSTALL_DATA) Docs/*.gif $(PREFIX)/$(DOCDIR)
	$(INSTALL_DATA) Docs/*.jpg $(PREFIX)/$(DOCDIR)
	$(INSTALL_DATA) Docs/*.xpm $(PREFIX)/$(DOCDIR)
	@mkdir -p $(PREFIX)/$(DOCDIR)/logos
	$(INSTALL_DATA) Docs/logos/* $(PREFIX)/$(DOCDIR)/logos

# Local Variables:
# tab-width: 2
# End:
