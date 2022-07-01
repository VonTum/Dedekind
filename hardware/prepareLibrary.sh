#!/bin/bash

source fpgaModules.sh

aocl library hdl-comp-pkg pcoeffProcessor.xml -o pcoeffProcessor.aoco
aocl library create -name pcoeffProcessor pcoeffProcessor.aoco
