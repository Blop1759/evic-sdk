#not the cleaniest way to handle it, but it's a way
#make default : must be cleaned before in order to link right objects !
all: clean
	@$(MAKE) -f Make.mk 
#make specific presa lib 
presa75: presa75_clean
	@$(MAKE) -f PresaTC75W.mk
#make specific vtc mini lib lib
vtcmini: vtcmini_clean
	@$(MAKE) -f EvicVTCMini.mk
#make docs : no need to specify modtype
docs:
	@$(MAKE) -f Make.mk $@
#make env_check default : no need to specify modtype
env_check:
	@$(MAKE) -f Make.mk $@
#make gen_tag default : no need to specify modtype
gen_tag:
	@$(MAKE) -f Make.mk $@
#make env_check presa
presa75_env_check:
	@$(MAKE) -f PresaTC75W.mk env_check
#make gen_tag presa
presa75_gen_tag:
	@$(MAKE) -f PresaTC75W.mk gentag
#make clean presa
presa75_clean:
	@$(MAKE) -f PresaTC75W.mk clean
#make clean vtcmini
vtcmini_clean:
	@$(MAKE) -f EvicVTCMini.mk clean
#make clean all
clean:
	@$(MAKE) -f Make.mk cleanall


.PHONY: all presa75 vtcmini presa75_clean vtcmini_clean clean docs presa75_env_check vtcmini_env_check presa75_gen_tag vtcmini_gen_tag env_check gen_tag
