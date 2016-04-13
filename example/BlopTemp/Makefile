TARGET := Blop
OBJS := main.o
export TARGET
export OBJS

all:
	@$(MAKE) -f $(EVICSDK)/make/Base.mk
presa75:
	@$(MAKE) -f PresaTC75W.mk
vtcmini:
	@$(MAKE) -f EvicVTCMini.mk
clean:
	@$(MAKE) -f $(EVICSDK)/make/Base.mk clean
  
.PHONY: all presa75 vtcmini clean
