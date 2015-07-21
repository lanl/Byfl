###################################
# Build all of Byfl               #
#                                 #
# By Scott Pakin <pakin@lanl.gov> #
###################################

SUBDIRS = include lib tools tests

EXTRA_DIST = README.md INSTALL.md LICENSE.md gen_opcode2name

# Sneaky trick: Output a sed command that will fully expand the prefix
# and exec_prefix variables in bf-gcc and bf-inst.
sed-command:
	@echo $(SED) \
	  -e 's,$${exec_prefix},$(exec_prefix),g' \
	  -e 's,$${prefix},$(prefix),g'
