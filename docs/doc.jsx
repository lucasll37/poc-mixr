import React, { useState, useEffect, useLayoutEffect, useMemo, useRef, useCallback } from "react";

/* ==================================================================== *
 * MIXR — Explorador de execução, EDL e classes built-in   (v6)
 *
 * MODEL, FACTORIES, SNIPPETS e STATS são GERADOS por scripts/extract_execution_chain.py
 * a partir da árvore de fontes. Nada aqui é digitado à mão:
 *   - herança      <- DECLARE_SUBCLASS nos headers
 *   - nome fábrica <- IMPLEMENT_*SUBCLASS nos .cpp
 *   - registro     <- name == X::getFactoryName() nos factory.cpp
 *   - slots        <- BEGIN_SLOTTABLE / END_SLOTTABLE
 *   - fases        <- definições de dynamics/transmit/receive/process
 *   - trechos      <- corpo do método, com arquivo e linha reais
 * ==================================================================== */

/* GERADO por scripts/extract_execution_chain.py a partir da arvore de fontes do MIXR. Nao editar. */
const MODEL = {"UsbJoystick":{"b":"IoDevice","f":null,"m":"linkage","r":true,"ch":["UsbJoystick","IoDevice","AbstractIoDevice"],"sl":["deviceIndex"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData","reset"],"hd":"src/linkage/platform/UsbJoystick_linux.hpp","src":"src/linkage/platform/UsbJoystick_msvc.cpp","ml":{"copyData":28,"reset":40}},"EmissionPduHandler":{"b":"Object","f":null,"m":"interop","r":true,"ch":["EmissionPduHandler"],"sl":["emitterName","emitterFunction","sensor","antenna","defaultIn","defaultOut"],"own":6,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/interop/dis/EmissionPduHandler.hpp","src":"src/interop/dis/EmissionPduHandler.cpp","ml":{"copyData":54,"deleteData":99}},"Ntm":{"b":"Object","f":null,"m":"interop","r":true,"ch":["Ntm"],"sl":["disEntityType","entityType","template"],"own":3,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/interop/common/Ntm.hpp","src":"src/interop/common/Ntm.cpp","ml":{"copyData":24,"deleteData":31}},"Nib":{"b":"Nib","f":"HlaNib","m":"interop","r":false,"ch":["Nib"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","shutdownNotification"],"hd":"include/mixr/interop/hla/Nib.hpp","src":"src/interop/hla/Nib.cpp","ml":{"copyData":51,"deleteData":128,"shutdownNotification":149}},"NetIO":{"b":"NetIO","f":"HlaNetIO","m":"interop","r":true,"ch":["NetIO"],"sl":["netInput","netOutput","version","maxTimeDR","maxPositionError","maxOrientationError","maxAge","maxEntityRange","emissionPduHandlers","siteID","applicationID","exerciseID","networkID","federationName","federateName","enableInput","enableOutput","enableRelay","timeline","inputEntityTypes","outputEntityTypes","fedFile","regulatingTime","constrainedTime"],"own":24,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","reset","shutdownNotification"],"hd":"include/mixr/interop/hla/NetIO.hpp","src":"src/interop/hla/NetIO.cpp","ml":{"copyData":76,"deleteData":164,"reset":174,"shutdownNotification":188}},"BaseEntity":{"b":"Object","f":null,"m":"interop","r":false,"ch":["BaseEntity"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/interop/rprfom/RprFom.hpp","src":"src/interop/rprfom/RprFom.cpp","ml":{"copyData":15}},"PhysicalEntity":{"b":"BaseEntity","f":null,"m":"interop","r":false,"ch":["PhysicalEntity","BaseEntity"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/interop/rprfom/RprFom.hpp","src":"src/interop/rprfom/RprFom.cpp","ml":{"copyData":28}},"Lifeform":{"b":"PhysicalEntity","f":null,"m":"interop","r":false,"ch":["Lifeform","PhysicalEntity","BaseEntity"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/interop/rprfom/RprFom.hpp","src":"src/interop/rprfom/RprFom.cpp","ml":{"copyData":41}},"Human":{"b":"Lifeform","f":null,"m":"interop","r":false,"ch":["Human","Lifeform","PhysicalEntity","BaseEntity"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/interop/rprfom/RprFom.hpp","src":"src/interop/rprfom/RprFom.cpp","ml":{"copyData":54}},"NonHuman":{"b":"Lifeform","f":null,"m":"interop","r":false,"ch":["NonHuman","Lifeform","PhysicalEntity","BaseEntity"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/interop/rprfom/RprFom.hpp","src":"src/interop/rprfom/RprFom.cpp","ml":{"copyData":67}},"Munition":{"b":"PhysicalEntity","f":null,"m":"interop","r":false,"ch":["Munition","PhysicalEntity","BaseEntity"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/interop/rprfom/RprFom.hpp","src":"src/interop/rprfom/RprFom.cpp","ml":{"copyData":80}},"Platform":{"b":"PhysicalEntity","f":null,"m":"interop","r":false,"ch":["Platform","PhysicalEntity","BaseEntity"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/interop/rprfom/RprFom.hpp","src":"src/interop/rprfom/RprFom.cpp","ml":{"copyData":93}},"Aircraft":{"b":"AirVehicle","f":null,"m":"models","r":true,"ch":["Aircraft","AirVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"Player"},"d":false,"ov":[],"hd":"include/mixr/models/player/air/Aircraft.hpp","src":"src/models/player/air/Aircraft.cpp","ml":{}},"AmphibiousVehicle":{"b":"Platform","f":null,"m":"interop","r":false,"ch":["AmphibiousVehicle","Platform","PhysicalEntity","BaseEntity"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/interop/rprfom/RprFom.hpp","src":"src/interop/rprfom/RprFom.cpp","ml":{}},"GroundVehicle":{"b":"Player","f":null,"m":"models","r":true,"ch":["GroundVehicle","Player","AbstractPlayer","Component"],"sl":["commandedPosition","up","down","launcherDownAngle","launcherUpAngle","launcherMoveTime"],"own":6,"wp":[0],"po":{"0":"GroundVehicle"},"d":false,"ov":["copyData","reset","dynamics"],"hd":"include/mixr/models/player/ground/GroundVehicle.hpp","src":"src/models/player/ground/GroundVehicle.cpp","ml":{"copyData":49,"reset":73,"dynamics":97}},"MultiDomainPlatform":{"b":"Platform","f":null,"m":"interop","r":false,"ch":["MultiDomainPlatform","Platform","PhysicalEntity","BaseEntity"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/interop/rprfom/RprFom.hpp","src":"src/interop/rprfom/RprFom.cpp","ml":{}},"Spacecraft":{"b":"Platform","f":null,"m":"interop","r":false,"ch":["Spacecraft","Platform","PhysicalEntity","BaseEntity"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/interop/rprfom/RprFom.hpp","src":"src/interop/rprfom/RprFom.cpp","ml":{}},"SubmersibleVessel":{"b":"Platform","f":null,"m":"interop","r":false,"ch":["SubmersibleVessel","Platform","PhysicalEntity","BaseEntity"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/interop/rprfom/RprFom.hpp","src":"src/interop/rprfom/RprFom.cpp","ml":{}},"SurfaceVessel":{"b":"Platform","f":null,"m":"interop","r":false,"ch":["SurfaceVessel","Platform","PhysicalEntity","BaseEntity"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/interop/rprfom/RprFom.hpp","src":"src/interop/rprfom/RprFom.cpp","ml":{}},"NtmInputNode":{"b":"Object","f":null,"m":"interop","r":false,"ch":["NtmInputNode"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/interop/common/NetIO.hpp","src":"src/interop/rprfom/NetIO.cpp","ml":{"copyData":338,"deleteData":362}},"NtmOutputNode":{"b":"Object","f":null,"m":"interop","r":false,"ch":["NtmOutputNode"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/interop/common/NetIO.hpp","src":null,"ml":{}},"IoData":{"b":"AbstractIoData","f":null,"m":"linkage","r":true,"ch":["IoData","AbstractIoData"],"sl":["numAI","numAO","numDI","numDO"],"own":4,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/linkage/IoData.hpp","src":"src/linkage/IoData.cpp","ml":{"copyData":35}},"IoDevice":{"b":"AbstractIoDevice","f":"BaseIoDevice","m":"linkage","r":false,"ch":["IoDevice","AbstractIoDevice"],"sl":["adapters"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/linkage/IoDevice.hpp","src":"src/linkage/IoDevice.cpp","ml":{"copyData":34}},"MockDevice":{"b":"AbstractIoDevice","f":null,"m":"linkage","r":true,"ch":["MockDevice","AbstractIoDevice"],"sl":["generators"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/linkage/MockDevice.hpp","src":"src/linkage/MockDevice.cpp","ml":{"copyData":29}},"IoHandler":{"b":"AbstractIoHandler","f":"BaseIoHandler","m":"linkage","r":false,"ch":["IoHandler","AbstractIoHandler","Component"],"sl":["ioData","inputData","outputData","devices","rate","priority"],"own":6,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","reset","shutdownNotification"],"hd":"include/mixr/linkage/IoHandler.hpp","src":"src/linkage/IoHandler.cpp","ml":{"copyData":42,"deleteData":90,"reset":102,"shutdownNotification":122}},"AnalogInputFixed":{"b":"AbstractGenerator","f":null,"m":"linkage","r":true,"ch":["AnalogInputFixed","AbstractGenerator"],"sl":["ai","value"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/linkage/generators/AnalogInputFixed.hpp","src":"src/linkage/generators/AnalogInputFixed.cpp","ml":{"copyData":30}},"AnalogSignalGen":{"b":"AbstractGenerator","f":null,"m":"linkage","r":true,"ch":["AnalogSignalGen","AbstractGenerator"],"sl":["ai","signal","frequency","phase"],"own":4,"wp":[],"po":{},"d":false,"ov":["copyData","reset"],"hd":"include/mixr/linkage/generators/AnalogSignalGen.hpp","src":"src/linkage/generators/AnalogSignalGen.cpp","ml":{"copyData":42,"reset":52}},"AbstractGenerator":{"b":"Object","f":null,"m":"linkage","r":false,"ch":["AbstractGenerator"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/linkage/generators/AbstractGenerator.hpp","src":"src/linkage/generators/AbstractGenerator.cpp","ml":{}},"DiscreteInputFixed":{"b":"AbstractGenerator","f":null,"m":"linkage","r":true,"ch":["DiscreteInputFixed","AbstractGenerator"],"sl":["di","signal"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/linkage/generators/DiscreteInputFixed.hpp","src":"src/linkage/generators/DiscreteInputFixed.cpp","ml":{"copyData":36}},"DiscreteOutput":{"b":"AbstractAdapter","f":null,"m":"linkage","r":true,"ch":["DiscreteOutput","AbstractAdapter"],"sl":["do","port","channel","inverted"],"own":4,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/linkage/adapters/DiscreteOutput.hpp","src":"src/linkage/adapters/DiscreteOutput.cpp","ml":{"copyData":36}},"AnalogInput":{"b":"AbstractAdapter","f":null,"m":"linkage","r":true,"ch":["AnalogInput","AbstractAdapter"],"sl":["ai","channel","deadband","offset","gain","table"],"own":6,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/linkage/adapters/AnalogInput.hpp","src":"src/linkage/adapters/AnalogInput.cpp","ml":{"copyData":41,"deleteData":60}},"AbstractAdapter":{"b":"Object","f":null,"m":"linkage","r":false,"ch":["AbstractAdapter"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/linkage/adapters/AbstractAdapter.hpp","src":"src/linkage/adapters/AbstractAdapter.cpp","ml":{}},"AnalogOutput":{"b":"AbstractAdapter","f":null,"m":"linkage","r":true,"ch":["AnalogOutput","AbstractAdapter"],"sl":["ao","channel","offset","gain","table"],"own":5,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/linkage/adapters/AnalogOutput.hpp","src":"src/linkage/adapters/AnalogOutput.cpp","ml":{"copyData":39,"deleteData":57}},"DiscreteInput":{"b":"AbstractAdapter","f":null,"m":"linkage","r":true,"ch":["DiscreteInput","AbstractAdapter"],"sl":["di","port","channel","inverted"],"own":4,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/linkage/adapters/DiscreteInput.hpp","src":"src/linkage/adapters/DiscreteInput.cpp","ml":{"copyData":36}},"Ai2DiSwitch":{"b":"AbstractAdapter","f":null,"m":"linkage","r":true,"ch":["Ai2DiSwitch","AbstractAdapter"],"sl":["di","channel","level","inverted"],"own":4,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/linkage/adapters/Ai2DiSwitch.hpp","src":"src/linkage/adapters/Ai2DiSwitch.cpp","ml":{"copyData":36}},"DataRecorder":{"b":"AbstractDataRecorder","f":null,"m":"recorder","r":true,"ch":["DataRecorder","AbstractDataRecorder","AbstractRecorderComponent","Component"],"sl":["outputHandler","eventName","application","mainSimExec","caseNum","missionNum","subjectNum","runNum","day","month","year"],"own":11,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","reset","shutdownNotification"],"hd":"include/mixr/recorder/DataRecorder.hpp","src":"src/recorder/DataRecorder.cpp","ml":{"copyData":81,"deleteData":102,"reset":140,"shutdownNotification":154}},"InputHandler":{"b":"AbstractRecorderComponent","f":"RecorderInputHandler","m":"recorder","r":false,"ch":["InputHandler","AbstractRecorderComponent","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/recorder/InputHandler.hpp","src":"src/recorder/InputHandler.cpp","ml":{}},"DataRecordHandle":{"b":"Object","f":null,"m":"recorder","r":false,"ch":["DataRecordHandle"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/recorder/DataRecordHandle.hpp","src":"src/recorder/DataRecordHandle.cpp","ml":{"copyData":22,"deleteData":31}},"TabPrinter":{"b":"PrintHandler","f":null,"m":"recorder","r":true,"ch":["TabPrinter","PrintHandler","OutputHandler","AbstractRecorderComponent","Component"],"sl":["msgHdrOptn","divider"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/recorder/TabPrinter.hpp","src":"src/recorder/TabPrinter.cpp","ml":{"copyData":32}},"OutputHandler":{"b":"AbstractRecorderComponent","f":"RecorderOutputHandler","m":"recorder","r":true,"ch":["OutputHandler","AbstractRecorderComponent","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","shutdownNotification","processComponents"],"hd":"include/mixr/recorder/OutputHandler.hpp","src":"src/recorder/OutputHandler.cpp","ml":{"copyData":21,"deleteData":31,"shutdownNotification":42,"processComponents":150}},"NetOutput":{"b":"OutputHandler","f":"RecorderNetOutput","m":"recorder","r":true,"ch":["NetOutput","OutputHandler","AbstractRecorderComponent","Component"],"sl":["netHandler","noWait"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/recorder/NetOutput.hpp","src":"src/recorder/NetOutput.cpp","ml":{"copyData":29,"deleteData":39}},"PrintPlayer":{"b":"PrintHandler","f":null,"m":"recorder","r":true,"ch":["PrintPlayer","PrintHandler","OutputHandler","AbstractRecorderComponent","Component"],"sl":["playerName"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/recorder/PrintPlayer.hpp","src":"src/recorder/PrintPlayer.cpp","ml":{"copyData":30,"deleteData":42}},"PrintHandler":{"b":"OutputHandler","f":null,"m":"recorder","r":false,"ch":["PrintHandler","OutputHandler","AbstractRecorderComponent","Component"],"sl":["filename","pathname"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/recorder/PrintHandler.hpp","src":"src/recorder/PrintHandler.cpp","ml":{"copyData":30,"deleteData":50}},"PrintSelected":{"b":"PrintHandler","f":null,"m":"recorder","r":true,"ch":["PrintSelected","PrintHandler","OutputHandler","AbstractRecorderComponent","Component"],"sl":["messageToken","fieldName","compareToValS","compareToValI","compareToValD","condition","timeOnly"],"own":7,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/recorder/PrintSelected.hpp","src":"src/recorder/PrintSelected.cpp","ml":{"copyData":51}},"FileReader":{"b":"Object","f":null,"m":"base","r":true,"ch":["FileReader"],"sl":["filename","pathname","recordLength"],"own":3,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/FileReader.hpp","src":"src/base/FileReader.cpp","ml":{"copyData":35,"deleteData":51}},"NetInput":{"b":"InputHandler","f":"RecorderNetInput","m":"recorder","r":true,"ch":["NetInput","InputHandler","AbstractRecorderComponent","Component"],"sl":["netHandler","noWait"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/recorder/NetInput.hpp","src":"src/recorder/NetInput.cpp","ml":{"copyData":35,"deleteData":49}},"FileWriter":{"b":"OutputHandler","f":"RecorderFileWriter","m":"recorder","r":true,"ch":["FileWriter","OutputHandler","AbstractRecorderComponent","Component"],"sl":["filename","pathname"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","shutdownNotification"],"hd":"include/mixr/recorder/FileWriter.hpp","src":"src/recorder/FileWriter.cpp","ml":{"copyData":33,"deleteData":52,"shutdownNotification":68}},"IrQueryMsg":{"b":"SensorMsg","f":null,"m":"models","r":false,"ch":["IrQueryMsg","SensorMsg"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/IrQueryMsg.hpp","src":"src/models/IrQueryMsg.cpp","ml":{"copyData":20,"deleteData":57}},"IrSignature":{"b":"Component","f":null,"m":"models","r":true,"ch":["IrSignature","Component"],"sl":["binSizes","irShapeSignature","baseHeatSignature","emissivity","effectiveArea"],"own":5,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/IrSignature.hpp","src":"src/models/IrSignature.cpp","ml":{"copyData":46,"deleteData":71}},"RfSignature":{"b":"Component","f":"Signature","m":"models","r":false,"ch":["RfSignature","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/models/Signatures.hpp","src":"src/models/Signatures.cpp","ml":{"copyData":33}},"SigConstant":{"b":"RfSignature","f":null,"m":"models","r":true,"ch":["SigConstant","RfSignature","Component"],"sl":["rcs"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/models/Signatures.hpp","src":"src/models/Signatures.cpp","ml":{"copyData":71}},"SigSphere":{"b":"RfSignature","f":null,"m":"models","r":true,"ch":["SigSphere","RfSignature","Component"],"sl":["radius"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/models/Signatures.hpp","src":"src/models/Signatures.cpp","ml":{"copyData":128}},"SigPlate":{"b":"RfSignature","f":null,"m":"models","r":true,"ch":["SigPlate","RfSignature","Component"],"sl":["a","b"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/Signatures.hpp","src":"src/models/Signatures.cpp","ml":{"copyData":192,"deleteData":199}},"SigDihedralCR":{"b":"SigPlate","f":null,"m":"models","r":true,"ch":["SigDihedralCR","SigPlate","RfSignature","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/Signatures.hpp","src":"src/models/Signatures.cpp","ml":{"copyData":284,"deleteData":289}},"SigTrihedralCR":{"b":"SigDihedralCR","f":null,"m":"models","r":true,"ch":["SigTrihedralCR","SigDihedralCR","SigPlate","RfSignature","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/models/Signatures.hpp","src":"src/models/Signatures.cpp","ml":{}},"SigSwitch":{"b":"RfSignature","f":null,"m":"models","r":true,"ch":["SigSwitch","RfSignature","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/models/Signatures.hpp","src":"src/models/Signatures.cpp","ml":{}},"SigAzEl":{"b":"RfSignature","f":null,"m":"models","r":true,"ch":["SigAzEl","RfSignature","Component"],"sl":["table","swapOrder","inDegrees","inDecibel"],"own":4,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/Signatures.hpp","src":"src/models/Signatures.cpp","ml":{"copyData":427,"deleteData":441}},"AircraftIrSignature":{"b":"IrSignature","f":null,"m":"models","r":true,"ch":["AircraftIrSignature","IrSignature","Component"],"sl":["airframeSignatureTable","airframeWavebandFactorTable","plumeSignatureTable","plumeWavebandFactorTable","hotPartsSignatureTable","hotPartsWavebandFactorTable"],"own":6,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/AircraftIrSignature.hpp","src":"src/models/AircraftIrSignature.cpp","ml":{"copyData":82,"deleteData":137}},"Emission":{"b":"SensorMsg","f":null,"m":"models","r":false,"ch":["Emission","SensorMsg"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/Emission.hpp","src":"src/models/Emission.cpp","ml":{"copyData":17,"deleteData":42}},"WorldModel":{"b":"Simulation","f":null,"m":"models","r":true,"ch":["WorldModel","Simulation","Component"],"sl":["latitude","longitude","gamingAreaRange","earthModel","gamingAreaUseEarthModel","terrain","atmosphere"],"own":7,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","reset","shutdownNotification"],"hd":"include/mixr/models/WorldModel.hpp","src":"src/models/WorldModel.cpp","ml":{"copyData":70,"deleteData":106,"reset":112,"shutdownNotification":131}},"Designator":{"b":"Object","f":null,"m":"models","r":false,"ch":["Designator"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/models/Designator.hpp","src":"src/models/Designator.cpp","ml":{"copyData":16}},"IrShape":{"b":"Object","f":null,"m":"models","r":true,"ch":["IrShape"],"sl":["area"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/models/IrShapes.hpp","src":"src/models/IrShapes.cpp","ml":{"copyData":42}},"IrSphere":{"b":"IrShape","f":null,"m":"models","r":true,"ch":["IrSphere","IrShape"],"sl":["radius"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/models/IrShapes.hpp","src":"src/models/IrShapes.cpp","ml":{"copyData":98}},"IrBox":{"b":"IrShape","f":null,"m":"models","r":true,"ch":["IrBox","IrShape"],"sl":["x","y","z"],"own":3,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/IrShapes.hpp","src":"src/models/IrShapes.cpp","ml":{"copyData":149,"deleteData":157}},"Image":{"b":"Object","f":"SarImage","m":"models","r":false,"ch":["Image"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/Image.hpp","src":"src/models/Image.cpp","ml":{"copyData":16,"deleteData":34}},"Track":{"b":"Object","f":null,"m":"models","r":true,"ch":["Track"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/Track.hpp","src":"src/models/Track.cpp","ml":{"copyData":26,"deleteData":86}},"RfTrack":{"b":"Track","f":null,"m":"models","r":false,"ch":["RfTrack","Track"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/Track.hpp","src":"src/models/Track.cpp","ml":{"copyData":385,"deleteData":402}},"IrTrack":{"b":"Track","f":null,"m":"models","r":false,"ch":["IrTrack","Track"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/Track.hpp","src":"src/models/Track.cpp","ml":{"copyData":489,"deleteData":506}},"TargetData":{"b":"Object","f":null,"m":"models","r":true,"ch":["TargetData"],"sl":["enabled","completed","weaponType","quantity","manualAssign","stickType","stickDistance","interval","maxMissDistance","armDelay","angle","azimuth","velocity"],"own":13,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/TargetData.hpp","src":"src/models/TargetData.cpp","ml":{"copyData":67,"deleteData":86}},"Action":{"b":"AbstractAction","f":null,"m":"models","r":false,"ch":["Action","AbstractAction"],"sl":[],"own":0,"wp":[3],"po":{"3":"Action"},"d":false,"ov":["copyData","deleteData","process"],"hd":"include/mixr/models/Actions.hpp","src":"src/models/Actions.cpp","ml":{"copyData":30,"deleteData":40,"process":114}},"ActionImagingSar":{"b":"Action","f":null,"m":"models","r":true,"ch":["ActionImagingSar","Action","AbstractAction"],"sl":["sarLatitude","sarLongitude","sarElevation","resolution","imageSize"],"own":5,"wp":[3],"po":{"3":"ActionImagingSar"},"d":false,"ov":["copyData","deleteData","process"],"hd":"include/mixr/models/Actions.hpp","src":"src/models/Actions.cpp","ml":{"copyData":175,"deleteData":190,"process":250}},"ActionWeaponRelease":{"b":"Action","f":null,"m":"models","r":true,"ch":["ActionWeaponRelease","Action","AbstractAction"],"sl":["targetLatitude","targetLongitude","targetElevation","station"],"own":4,"wp":[3],"po":{"3":"Action"},"d":false,"ov":["copyData"],"hd":"include/mixr/models/Actions.hpp","src":"src/models/Actions.cpp","ml":{"copyData":445}},"ActionDecoyRelease":{"b":"Action","f":null,"m":"models","r":true,"ch":["ActionDecoyRelease","Action","AbstractAction"],"sl":["numToLaunch","interval"],"own":2,"wp":[3],"po":{"3":"ActionDecoyRelease"},"d":false,"ov":["copyData","process"],"hd":"include/mixr/models/Actions.hpp","src":"src/models/Actions.cpp","ml":{"copyData":579,"process":635}},"ActionCamouflageType":{"b":"Action","f":null,"m":"models","r":true,"ch":["ActionCamouflageType","Action","AbstractAction"],"sl":["camouflageType"],"own":1,"wp":[3],"po":{"3":"Action"},"d":false,"ov":["copyData"],"hd":"include/mixr/models/Actions.hpp","src":"src/models/Actions.cpp","ml":{"copyData":702}},"SensorMsg":{"b":"Object","f":null,"m":"models","r":false,"ch":["SensorMsg"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/SensorMsg.hpp","src":"src/models/SensorMsg.cpp","ml":{"copyData":26,"deleteData":59}},"SynchronizedState":{"b":"Object","f":null,"m":"models","r":false,"ch":["SynchronizedState"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/models/SynchronizedState.hpp","src":"src/models/SynchronizedState.cpp","ml":{"copyData":19}},"Tdb":{"b":"Object","f":"Gimbal_Tdb","m":"models","r":false,"ch":["Tdb"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/Tdb.hpp","src":"src/models/Tdb.cpp","ml":{"copyData":54,"deleteData":81}},"Message":{"b":"Object","f":null,"m":"models","r":false,"ch":["Message"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["deleteData","copyData"],"hd":"include/mixr/models/Message.hpp","src":"src/models/Message.cpp","ml":{"deleteData":23,"copyData":29}},"MultiActorAgent":{"b":"Component","f":null,"m":"models","r":true,"ch":["MultiActorAgent","Component"],"sl":["state","agentList"],"own":2,"wp":[],"po":{},"d":false,"ov":["deleteData","reset","updateData"],"hd":"include/mixr/models/MultiActorAgent.hpp","src":"src/models/MultiActorAgent.cpp","ml":{"deleteData":38,"reset":45,"updateData":66}},"SimAgent":{"b":"Agent","f":null,"m":"models","r":true,"ch":["SimAgent","Agent","Component"],"sl":["actorPlayerName","actorComponentName"],"own":2,"wp":[],"po":{},"d":false,"ov":["deleteData"],"hd":"include/mixr/models/SimAgent.hpp","src":"src/models/SimAgent.cpp","ml":{"deleteData":33}},"Tws":{"b":"Radar","f":null,"m":"models","r":true,"ch":["Tws","Radar","RfSensor","RfSystem","System","Component"],"sl":[],"own":0,"wp":[1,2,3],"po":{"1":"Radar","2":"Radar","3":"Radar"},"d":true,"ov":["copyData"],"hd":"include/mixr/models/sensor/Tws.hpp","src":"src/models/sensor/Tws.cpp","ml":{"copyData":17}},"Gmti":{"b":"Radar","f":null,"m":"models","r":true,"ch":["Gmti","Radar","RfSensor","RfSystem","System","Component"],"sl":["poi"],"own":1,"wp":[0,1,2,3],"po":{"0":"Gmti","1":"Radar","2":"Radar","3":"Radar"},"d":true,"ov":["copyData","dynamics"],"hd":"include/mixr/models/sensor/Gmti.hpp","src":"src/models/sensor/Gmti.cpp","ml":{"copyData":34,"dynamics":43}},"Stt":{"b":"Radar","f":null,"m":"models","r":true,"ch":["Stt","Radar","RfSensor","RfSystem","System","Component"],"sl":[],"own":0,"wp":[0,1,2,3],"po":{"0":"Stt","1":"Radar","2":"Radar","3":"Radar"},"d":true,"ov":["copyData","dynamics"],"hd":"include/mixr/models/sensor/Stt.hpp","src":"src/models/sensor/Stt.cpp","ml":{"copyData":21,"dynamics":29}},"Route":{"b":"Component","f":null,"m":"models","r":true,"ch":["Route","Component"],"sl":["to","TO","autoSequence","autoSeqDistance","wrap"],"own":5,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","reset","updateData","processComponents"],"hd":"include/mixr/models/navigation/Route.hpp","src":"src/models/navigation/Route.cpp","ml":{"copyData":50,"deleteData":70,"reset":79,"updateData":122,"processComponents":660}},"Gps":{"b":"Navigation","f":null,"m":"models","r":true,"ch":["Gps","Navigation","System","Component"],"sl":[],"own":0,"wp":[3],"po":{"3":"Navigation"},"d":true,"ov":["copyData"],"hd":"include/mixr/models/navigation/Gps.hpp","src":"src/models/navigation/Gps.cpp","ml":{"copyData":17}},"Ins":{"b":"Navigation","f":null,"m":"models","r":true,"ch":["Ins","Navigation","System","Component"],"sl":[],"own":0,"wp":[3],"po":{"3":"Navigation"},"d":true,"ov":["copyData"],"hd":"include/mixr/models/navigation/Ins.hpp","src":"src/models/navigation/Ins.cpp","ml":{"copyData":17}},"Steerpoint":{"b":"Component","f":null,"m":"models","r":true,"ch":["Steerpoint","Component"],"sl":["stptType","latitude","longitude","xPos","yPos","elevation","altitude","airspeed","pta","sca","description","magvar","next","action"],"own":14,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","reset","processComponents"],"hd":"include/mixr/models/navigation/Steerpoint.hpp","src":"src/models/navigation/Steerpoint.cpp","ml":{"copyData":93,"deleteData":171,"reset":178,"processComponents":737}},"Navigation":{"b":"System","f":null,"m":"models","r":true,"ch":["Navigation","System","Component"],"sl":["route","utc","feba","bullseye"],"own":4,"wp":[3],"po":{"3":"Navigation"},"d":true,"ov":["copyData","deleteData","reset","updateData","process"],"hd":"include/mixr/models/navigation/Navigation.hpp","src":"src/models/navigation/Navigation.cpp","ml":{"copyData":58,"deleteData":127,"reset":137,"updateData":180,"process":191}},"Bullseye":{"b":"Steerpoint","f":null,"m":"models","r":true,"ch":["Bullseye","Steerpoint","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/models/navigation/Bullseye.hpp","src":"src/models/navigation/Bullseye.cpp","ml":{}},"IrAtmosphere":{"b":"AbstractAtmosphere","f":null,"m":"models","r":true,"ch":["IrAtmosphere","AbstractAtmosphere","Component"],"sl":["waveBands","transmissivityTable1","skyRadiance","earthRadiance"],"own":4,"wp":[],"po":{},"d":false,"ov":["deleteData"],"hd":"include/mixr/models/environment/IrAtmosphere.hpp","src":"src/models/environment/IrAtmosphere.cpp","ml":{"deleteData":42}},"IrAtmosphere1":{"b":"IrAtmosphere","f":null,"m":"models","r":true,"ch":["IrAtmosphere1","IrAtmosphere","AbstractAtmosphere","Component"],"sl":["solarRadiationTable","backgroundRadiationTable","transmissivityTable"],"own":3,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/environment/IrAtmosphere1.hpp","src":"src/models/environment/IrAtmosphere1.cpp","ml":{"copyData":41,"deleteData":46}},"AbstractAtmosphere":{"b":"Component","f":null,"m":"models","r":false,"ch":["AbstractAtmosphere","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/models/environment/AbstractAtmosphere.hpp","src":"src/models/environment/AbstractAtmosphere.cpp","ml":{}},"LaeroModel":{"b":"AerodynamicsModel","f":null,"m":"models","r":true,"ch":["LaeroModel","AerodynamicsModel","DynamicsModel","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"LaeroModel"},"d":false,"ov":["copyData","dynamics","reset"],"hd":"include/mixr/models/dynamics/LaeroModel.hpp","src":"src/models/dynamics/LaeroModel.cpp","ml":{"copyData":36,"dynamics":90,"reset":96}},"AerodynamicsModel":{"b":"DynamicsModel","f":null,"m":"models","r":false,"ch":["AerodynamicsModel","DynamicsModel","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"DynamicsModel"},"d":false,"ov":["copyData"],"hd":"include/mixr/models/dynamics/AerodynamicsModel.hpp","src":"src/models/dynamics/AerodynamicsModel.cpp","ml":{"copyData":18}},"JSBSimModel":{"b":"AerodynamicsModel","f":null,"m":"models","r":true,"ch":["JSBSimModel","AerodynamicsModel","DynamicsModel","Component"],"sl":["rootDir","model","debugLevel"],"own":3,"wp":[0],"po":{"0":"JSBSimModel"},"d":false,"ov":["copyData","deleteData","dynamics","reset"],"hd":"include/mixr/models/dynamics/JSBSimModel.hpp","src":"src/models/dynamics/JSBSimModel.cpp","ml":{"copyData":65,"deleteData":96,"dynamics":620,"reset":772}},"RacModel":{"b":"AerodynamicsModel","f":null,"m":"models","r":true,"ch":["RacModel","AerodynamicsModel","DynamicsModel","Component"],"sl":["minSpeed","speedMaxG","maxg","maxAccel","cmdAltitude","cmdHeading","cmdSpeed"],"own":7,"wp":[0],"po":{"0":"RacModel"},"d":false,"ov":["copyData","reset","dynamics"],"hd":"include/mixr/models/dynamics/RacModel.hpp","src":"src/models/dynamics/RacModel.cpp","ml":{"copyData":48,"reset":62,"dynamics":70}},"SpaceDynamicsModel":{"b":"DynamicsModel","f":null,"m":"models","r":false,"ch":["SpaceDynamicsModel","DynamicsModel","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"DynamicsModel"},"d":false,"ov":[],"hd":"include/mixr/models/dynamics/SpaceDynamicsModel.hpp","src":"src/models/dynamics/SpaceDynamicsModel.cpp","ml":{}},"DynamicsModel":{"b":"Component","f":null,"m":"models","r":false,"ch":["DynamicsModel","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"DynamicsModel"},"d":false,"ov":["copyData","dynamics"],"hd":"include/mixr/models/dynamics/DynamicsModel.hpp","src":"src/models/dynamics/DynamicsModel.cpp","ml":{"copyData":17,"dynamics":26}},"RfSystem":{"b":"System","f":null,"m":"models","r":false,"ch":["RfSystem","System","Component"],"sl":["antennaName","frequency","bandwidth","powerPeak","threshold","noiseFigure","systemTemperature","lossXmit","lossRecv","lossSignalProcess","disableEmissions","bandwidthNoise"],"own":12,"wp":[3],"po":{"3":"RfSystem"},"d":true,"ov":["copyData","deleteData","shutdownNotification","reset","updateData","process","processPlayersOfInterest","rfReceivedEmission"],"hd":"include/mixr/models/system/RfSystem.hpp","src":"src/models/system/RfSystem.cpp","ml":{"copyData":57,"deleteData":92,"shutdownNotification":108,"reset":126,"updateData":160,"process":176,"processPlayersOfInterest":184,"rfReceivedEmission":207}},"ScanGimbal":{"b":"Gimbal","f":null,"m":"models","r":true,"ch":["ScanGimbal","Gimbal","System","Component"],"sl":["scanMode","leftToRightScan","scanWidth","searchVolume","reference","barSpacing","numBars","revolutionsPerSec","scanRadius","pseudoRandomPattern","maxRevolutions"],"own":11,"wp":[0],"po":{"0":"ScanGimbal"},"d":true,"ov":["copyData","reset","dynamics"],"hd":"include/mixr/models/system/ScanGimbal.hpp","src":"src/models/system/ScanGimbal.cpp","ml":{"copyData":63,"reset":94,"dynamics":103}},"Sar":{"b":"Radar","f":null,"m":"models","r":true,"ch":["Sar","Radar","RfSensor","RfSystem","System","Component"],"sl":["chipSize"],"own":1,"wp":[1,2,3],"po":{"1":"Radar","2":"Radar","3":"Sar"},"d":true,"ov":["copyData","deleteData","process"],"hd":"include/mixr/models/system/Sar.hpp","src":"src/models/system/Sar.cpp","ml":{"copyData":42,"deleteData":75,"process":188}},"Radio":{"b":"RfSystem","f":null,"m":"models","r":true,"ch":["Radio","RfSystem","System","Component"],"sl":["numChannels","channels","channel","maxDetectRange","radioID"],"own":5,"wp":[2,3],"po":{"2":"Radio","3":"RfSystem"},"d":true,"ov":["copyData","deleteData","receive"],"hd":"include/mixr/models/system/Radio.hpp","src":"src/models/system/Radio.cpp","ml":{"copyData":40,"deleteData":60,"receive":212}},"System":{"b":"Component","f":null,"m":"models","r":true,"ch":["System","Component"],"sl":["powerSwitch","OFF","STBY","ON"],"own":4,"wp":[],"po":{},"d":true,"ov":["copyData","deleteData","reset","updateData","updateTC","dynamics","transmit","receive","process"],"hd":"include/mixr/models/system/System.hpp","src":"src/models/system/System.cpp","ml":{"copyData":34,"deleteData":44,"reset":62,"updateData":73,"updateTC":84,"dynamics":135,"transmit":139,"receive":143,"process":147}},"StoresMgr":{"b":"Stores","f":"BaseStoresMgr","m":"models","r":false,"ch":["StoresMgr","Stores","ExternalStore","System","Component"],"sl":[],"own":0,"wp":[3],"po":{"3":"StoresMgr"},"d":true,"ov":["copyData","deleteData","process","shutdownNotification"],"hd":"include/mixr/models/system/StoresMgr.hpp","src":"src/models/system/StoresMgr.cpp","ml":{"copyData":45,"deleteData":54,"process":62,"shutdownNotification":76}},"SensorMgr":{"b":"RfSensor","f":null,"m":"models","r":true,"ch":["SensorMgr","RfSensor","RfSystem","System","Component"],"sl":[],"own":0,"wp":[3],"po":{"3":"RfSystem"},"d":true,"ov":[],"hd":"include/mixr/models/system/SensorMgr.hpp","src":"src/models/system/SensorMgr.cpp","ml":{}},"MergingIrSensor":{"b":"IrSensor","f":null,"m":"models","r":true,"ch":["MergingIrSensor","IrSensor","IrSystem","System","Component"],"sl":["azimuthBin","elevationBin"],"own":2,"wp":[1,2,3],"po":{"1":"IrSensor","2":"MergingIrSensor","3":"IrSensor"},"d":true,"ov":["copyData","reset","receive"],"hd":"include/mixr/models/system/MergingIrSensor.hpp","src":"src/models/system/MergingIrSensor.cpp","ml":{"copyData":37,"reset":47,"receive":62}},"IrSensor":{"b":"IrSystem","f":null,"m":"models","r":true,"ch":["IrSensor","IrSystem","System","Component"],"sl":["lowerWavelength","upperWavelength","nei","threshold","IFOV","sensorType","FOR","azimuthBin","elevationBin","maximumRange","trackManagerName"],"own":11,"wp":[1,3],"po":{"1":"IrSensor","3":"IrSensor"},"d":true,"ov":["copyData","deleteData","shutdownNotification","reset","updateData","transmit","process"],"hd":"include/mixr/models/system/IrSensor.hpp","src":"src/models/system/IrSensor.cpp","ml":{"copyData":82,"deleteData":112,"shutdownNotification":122,"reset":132,"updateData":164,"transmit":194,"process":372}},"Radar":{"b":"RfSensor","f":null,"m":"models","r":true,"ch":["Radar","RfSensor","RfSystem","System","Component"],"sl":["igain"],"own":1,"wp":[1,2,3],"po":{"1":"Radar","2":"Radar","3":"Radar"},"d":true,"ov":["copyData","deleteData","shutdownNotification","updateData","reset","transmit","receive","process"],"hd":"include/mixr/models/system/Radar.hpp","src":"src/models/system/Radar.cpp","ml":{"copyData":59,"deleteData":79,"shutdownNotification":87,"updateData":122,"reset":133,"transmit":153,"receive":184,"process":325}},"IrSeeker":{"b":"ScanGimbal","f":null,"m":"models","r":true,"ch":["IrSeeker","ScanGimbal","Gimbal","System","Component"],"sl":[],"own":0,"wp":[0,3],"po":{"0":"ScanGimbal","3":"IrSeeker"},"d":true,"ov":["deleteData","shutdownNotification","reset","processPlayersOfInterest","process"],"hd":"include/mixr/models/system/IrSeeker.hpp","src":"src/models/system/IrSeeker.cpp","ml":{"deleteData":73,"shutdownNotification":81,"reset":90,"processPlayersOfInterest":101,"process":116}},"TdbIr":{"b":"Tdb","f":"Seeker_TdbIr","m":"models","r":false,"ch":["TdbIr","Tdb"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/models/system/IrSeeker.hpp","src":"src/models/system/IrSeeker.cpp","ml":{"copyData":326}},"Jammer":{"b":"RfSensor","f":null,"m":"models","r":true,"ch":["Jammer","RfSensor","RfSystem","System","Component"],"sl":[],"own":0,"wp":[1,3],"po":{"1":"Jammer","3":"RfSystem"},"d":true,"ov":["copyData","transmit"],"hd":"include/mixr/models/system/Jammer.hpp","src":"src/models/system/Jammer.cpp","ml":{"copyData":29,"transmit":37}},"AvionicsPod":{"b":"ExternalStore","f":null,"m":"models","r":true,"ch":["AvionicsPod","ExternalStore","System","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":true,"ov":[],"hd":"include/mixr/models/system/AvionicsPod.hpp","src":"src/models/system/AvionicsPod.cpp","ml":{}},"Autopilot":{"b":"Pilot","f":null,"m":"models","r":true,"ch":["Autopilot","Pilot","System","Component"],"sl":["navMode","holdAltitude","altitudeHoldMode","holdVelocityKts","velocityHoldMode","holdHeading","headingHoldMode","loiterMode","loiterPatternLength","loiterPatternCcwFlag","leadFollowingDistanceTrail","leadFollowingDistanceRight","leadFollowingDeltaAltitude","leadPlayerName","followTheLeadMode","Follow the lead","maxRateOfTurnDps","maxBankAngle","maxClimbRateFpm","maxClimbRateMps","maxPitchAngle","loiterPatternTime","maxAcceleration"],"own":23,"wp":[3],"po":{"3":"Autopilot"},"d":true,"ov":["copyData","deleteData","shutdownNotification","reset","process"],"hd":"include/mixr/models/system/Autopilot.hpp","src":"src/models/system/Autopilot.cpp","ml":{"copyData":106,"deleteData":161,"shutdownNotification":166,"reset":172,"process":187}},"CollisionDetect":{"b":"System","f":null,"m":"models","r":true,"ch":["CollisionDetect","System","Component"],"sl":["collisionRange","maxPlayers","playerTypes","maxRange2Players","maxAngle2Players","localOnly","useWorldCoordinates","sendCrashEvents"],"own":8,"wp":[3],"po":{"3":"CollisionDetect"},"d":true,"ov":["copyData","deleteData","updateData","process"],"hd":"include/mixr/models/system/CollisionDetect.hpp","src":"src/models/system/CollisionDetect.cpp","ml":{"copyData":52,"deleteData":69,"updateData":150,"process":298}},"Iff":{"b":"Radio","f":null,"m":"models","r":true,"ch":["Iff","Radio","RfSystem","System","Component"],"sl":["mode1","mode2","mode3a","mode4a","mode4b","enableMode1","enableMode2","enableMode3a","enableMode4","whichMode4","enableModeC"],"own":11,"wp":[2,3],"po":{"2":"Radio","3":"RfSystem"},"d":true,"ov":["copyData","reset"],"hd":"include/mixr/models/system/Iff.hpp","src":"src/models/system/Iff.cpp","ml":{"copyData":49,"reset":79}},"Gimbal":{"b":"System","f":null,"m":"models","r":true,"ch":["Gimbal","System","Component"],"sl":["type","mechanical","electronic","location","initPosition","initPosAzimuth","initPosElevation","initPosRoll","azimuthLimits","azimuthLimitLeft","azimuthLimitRight","elevationLimits","elevationLimitLower","elevationLimitUpper","rollLimits","rollLimitLower","rollLimitUpper","maxRates","maxRateAzimuth","maxRateElevation","maxRateRoll","commandPosition","commandPosAzimuth","commandPosElevation","commandPosRoll","commandRates","commandRateAzimuth","commandRateElevation","commandRateRoll","terrainOcculting","checkHorizon","playerOfInterestTypes","air","ground","weapon","ship","building","lifeform","space","maxPlayersOfInterest","maxRange2PlayersOfInterest","maxAngle2PlayersOfInterest","localPlayersOfInterestOnly","useWorldCoordinates","ownHeadingOnly"],"own":45,"wp":[0],"po":{"0":"Gimbal"},"d":true,"ov":["copyData","deleteData","reset","shutdownNotification","dynamics","processPlayersOfInterest"],"hd":"include/mixr/models/system/Gimbal.hpp","src":"src/models/system/Gimbal.cpp","ml":{"copyData":156,"deleteData":191,"reset":199,"shutdownNotification":211,"dynamics":221,"processPlayersOfInterest":1318}},"SimpleStoresMgr":{"b":"StoresMgr","f":"StoresMgr","m":"models","r":true,"ch":["SimpleStoresMgr","StoresMgr","Stores","ExternalStore","System","Component"],"sl":[],"own":0,"wp":[3],"po":{"3":"SimpleStoresMgr"},"d":true,"ov":["copyData","deleteData","process","updateData"],"hd":"include/mixr/models/system/SimpleStoresMgr.hpp","src":"src/models/system/SimpleStoresMgr.cpp","ml":{"copyData":35,"deleteData":44,"process":52,"updateData":66}},"ExternalStore":{"b":"System","f":null,"m":"models","r":false,"ch":["ExternalStore","System","Component"],"sl":["type","jettisonable"],"own":2,"wp":[],"po":{},"d":true,"ov":["copyData","deleteData","reset"],"hd":"include/mixr/models/system/ExternalStore.hpp","src":"src/models/system/ExternalStore.cpp","ml":{"copyData":31,"deleteData":46,"reset":54}},"Pilot":{"b":"System","f":null,"m":"models","r":true,"ch":["Pilot","System","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":true,"ov":[],"hd":"include/mixr/models/system/Pilot.hpp","src":"src/models/system/Pilot.cpp","ml":{}},"OnboardComputer":{"b":"System","f":null,"m":"models","r":true,"ch":["OnboardComputer","System","Component"],"sl":[],"own":0,"wp":[3],"po":{"3":"OnboardComputer"},"d":true,"ov":["copyData","deleteData","shutdownNotification","reset","process","updateData"],"hd":"include/mixr/models/system/OnboardComputer.hpp","src":"src/models/system/OnboardComputer.cpp","ml":{"copyData":23,"deleteData":33,"shutdownNotification":42,"reset":52,"process":71,"updateData":79}},"Gun":{"b":"ExternalStore","f":null,"m":"models","r":true,"ch":["Gun","ExternalStore","System","Component"],"sl":["bulletType","rounds","unlimited","rate","burstRate","position","roll","pitch","yaw"],"own":9,"wp":[3],"po":{"3":"Gun"},"d":true,"ov":["copyData","deleteData","shutdownNotification","reset","process"],"hd":"include/mixr/models/system/Guns.hpp","src":"src/models/system/Guns.cpp","ml":{"copyData":69,"deleteData":99,"shutdownNotification":104,"reset":118,"process":136}},"RfSensor":{"b":"RfSystem","f":null,"m":"models","r":true,"ch":["RfSensor","RfSystem","System","Component"],"sl":["trackManagerName","modes","ranges","initRangeIdx","PRF","pulseWidth","beamWidth","typeId","syncXmitWithScan"],"own":9,"wp":[3],"po":{"3":"RfSystem"},"d":true,"ov":["copyData","deleteData","shutdownNotification","reset","updateData"],"hd":"include/mixr/models/system/RfSensor.hpp","src":"src/models/system/RfSensor.cpp","ml":{"copyData":67,"deleteData":103,"shutdownNotification":119,"reset":135,"updateData":200}},"FuelTank":{"b":"ExternalStore","f":null,"m":"models","r":true,"ch":["FuelTank","ExternalStore","System","Component"],"sl":["fuelWt","capacity"],"own":2,"wp":[],"po":{},"d":true,"ov":["copyData","reset"],"hd":"include/mixr/models/system/FuelTank.hpp","src":"src/models/system/FuelTank.cpp","ml":{"copyData":26,"reset":40}},"Datalink":{"b":"System","f":null,"m":"models","r":true,"ch":["Datalink","System","Component"],"sl":["radioId","maxRange","radioName","trackManagerName"],"own":4,"wp":[0],"po":{"0":"Datalink"},"d":true,"ov":["copyData","deleteData","shutdownNotification","reset","dynamics"],"hd":"include/mixr/models/system/Datalink.hpp","src":"src/models/system/Datalink.cpp","ml":{"copyData":54,"deleteData":85,"shutdownNotification":103,"reset":209,"dynamics":263}},"Rwr":{"b":"RfSensor","f":null,"m":"models","r":true,"ch":["Rwr","RfSensor","RfSystem","System","Component"],"sl":[],"own":0,"wp":[2,3],"po":{"2":"Rwr","3":"Rwr"},"d":true,"ov":["copyData","deleteData","shutdownNotification","receive","process"],"hd":"include/mixr/models/system/Rwr.hpp","src":"src/models/system/Rwr.cpp","ml":{"copyData":50,"deleteData":55,"shutdownNotification":64,"receive":74,"process":172}},"CommRadio":{"b":"Radio","f":null,"m":"models","r":true,"ch":["CommRadio","Radio","RfSystem","System","Component"],"sl":[],"own":0,"wp":[2,3],"po":{"2":"Radio","3":"RfSystem"},"d":true,"ov":["copyData","deleteData"],"hd":"include/mixr/models/system/CommRadio.hpp","src":"src/models/system/CommRadio.cpp","ml":{"copyData":18,"deleteData":26}},"Stores":{"b":"ExternalStore","f":null,"m":"models","r":true,"ch":["Stores","ExternalStore","System","Component"],"sl":["numStations","stores","selected"],"own":3,"wp":[3],"po":{"3":"Stores"},"d":true,"ov":["copyData","deleteData","reset","process","updateTC","updateData","releaseWeapon"],"hd":"include/mixr/models/system/Stores.hpp","src":"src/models/system/Stores.cpp","ml":{"copyData":38,"deleteData":47,"reset":56,"process":72,"updateTC":83,"updateData":108,"releaseWeapon":330}},"StabilizingGimbal":{"b":"Gimbal","f":null,"m":"models","r":true,"ch":["StabilizingGimbal","Gimbal","System","Component"],"sl":["stabilizingMode"],"own":1,"wp":[0],"po":{"0":"StabilizingGimbal"},"d":true,"ov":["copyData","dynamics"],"hd":"include/mixr/models/system/StabilizingGimbal.hpp","src":"src/models/system/StabilizingGimbal.cpp","ml":{"copyData":39,"dynamics":50}},"IrSystem":{"b":"System","f":null,"m":"models","r":false,"ch":["IrSystem","System","Component"],"sl":["seekerName","disableQueries"],"own":2,"wp":[],"po":{},"d":true,"ov":["copyData","deleteData","shutdownNotification","reset","updateData","processPlayersOfInterest"],"hd":"include/mixr/models/system/IrSystem.hpp","src":"src/models/system/IrSystem.cpp","ml":{"copyData":34,"deleteData":46,"shutdownNotification":55,"reset":64,"updateData":92,"processPlayersOfInterest":133}},"Antenna":{"b":"ScanGimbal","f":null,"m":"models","r":true,"ch":["Antenna","ScanGimbal","Gimbal","System","Component"],"sl":["polarization","threshold","gain","gainPattern","gainPatternDeg","recycle","beamWidth"],"own":7,"wp":[0,3],"po":{"0":"ScanGimbal","3":"Antenna"},"d":true,"ov":["copyData","deleteData","reset","shutdownNotification","process","rfTransmit"],"hd":"include/mixr/models/system/Antenna.hpp","src":"src/models/system/Antenna.cpp","ml":{"copyData":81,"deleteData":103,"reset":111,"shutdownNotification":120,"process":131,"rfTransmit":379}},"AirTrkMgr":{"b":"TrackManager","f":null,"m":"models","r":true,"ch":["AirTrkMgr","TrackManager","System","Component"],"sl":["positionGate","rangeGate","velocityGate"],"own":3,"wp":[3],"po":{"3":"TrackManager"},"d":true,"ov":["copyData","deleteData"],"hd":"include/mixr/models/system/trackmanager/AirTrkMgr.hpp","src":"src/models/system/trackmanager/AirTrkMgr.cpp","ml":{"copyData":64,"deleteData":84}},"AngleOnlyTrackManager":{"b":"TrackManager","f":null,"m":"models","r":false,"ch":["AngleOnlyTrackManager","TrackManager","System","Component"],"sl":["azimuthBin","elevationBin"],"own":2,"wp":[3],"po":{"3":"TrackManager"},"d":true,"ov":["copyData","deleteData","shutdownNotification","newReport"],"hd":"include/mixr/models/system/trackmanager/AngleOnlyTrackManager.hpp","src":"src/models/system/trackmanager/AngleOnlyTrackManager.cpp","ml":{"copyData":65,"deleteData":75,"shutdownNotification":83,"newReport":121}},"RwrTrkMgr":{"b":"TrackManager","f":null,"m":"models","r":true,"ch":["RwrTrkMgr","TrackManager","System","Component"],"sl":[],"own":0,"wp":[3],"po":{"3":"TrackManager"},"d":true,"ov":["copyData","deleteData"],"hd":"include/mixr/models/system/trackmanager/RwrTrkMgr.hpp","src":"src/models/system/trackmanager/RwrTrkMgr.cpp","ml":{"copyData":53,"deleteData":69}},"GmtiTrkMgr":{"b":"TrackManager","f":null,"m":"models","r":true,"ch":["GmtiTrkMgr","TrackManager","System","Component"],"sl":[],"own":0,"wp":[3],"po":{"3":"TrackManager"},"d":true,"ov":["copyData","deleteData"],"hd":"include/mixr/models/system/trackmanager/GmtiTrkMgr.hpp","src":"src/models/system/trackmanager/GmtiTrkMgr.cpp","ml":{"copyData":52,"deleteData":68}},"TrackManager":{"b":"System","f":null,"m":"models","r":false,"ch":["TrackManager","System","Component"],"sl":["maxTracks","maxTrackAge","firstTrackId","alpha","beta","gamma","logTrackUpdates"],"own":7,"wp":[3],"po":{"3":"TrackManager"},"d":true,"ov":["copyData","deleteData","reset","shutdownNotification","process","newReport"],"hd":"include/mixr/models/system/trackmanager/TrackManager.hpp","src":"src/models/system/trackmanager/TrackManager.cpp","ml":{"copyData":68,"deleteData":95,"reset":134,"shutdownNotification":145,"process":155,"newReport":301}},"AirAngleOnlyTrkMgrPT":{"b":"AirAngleOnlyTrkMgr","f":null,"m":"models","r":false,"ch":["AirAngleOnlyTrkMgrPT","AirAngleOnlyTrkMgr","AngleOnlyTrackManager","TrackManager","System","Component"],"sl":[],"own":0,"wp":[3],"po":{"3":"TrackManager"},"d":true,"ov":["copyData"],"hd":"include/mixr/models/system/trackmanager/AirAngleOnlyTrkMgrPT.hpp","src":"src/models/system/trackmanager/AirAngleOnlyTrkMgrPT.cpp","ml":{"copyData":49}},"AirAngleOnlyTrkMgr":{"b":"AngleOnlyTrackManager","f":null,"m":"models","r":true,"ch":["AirAngleOnlyTrkMgr","AngleOnlyTrackManager","TrackManager","System","Component"],"sl":[],"own":0,"wp":[3],"po":{"3":"TrackManager"},"d":true,"ov":["copyData"],"hd":"include/mixr/models/system/trackmanager/AirAngleOnlyTrkMgr.hpp","src":"src/models/system/trackmanager/AirAngleOnlyTrkMgr.cpp","ml":{"copyData":36}},"Player":{"b":"AbstractPlayer","f":null,"m":"models","r":true,"ch":["Player","AbstractPlayer","Component"],"sl":["initXPos","initYPos","initAlt","initPosition","initLatitude","initLongitude","initGeocentric","initRoll","initPitch","initHeading","initEuler","initVelocity","initVelocityKts","type","F-16A","Tank","SA-6","side","signature","irSignature","camouflageType","terrainElevReq","interpolateTerrain","terrainOffset","positionFreeze","altitudeFreeze","attitudeFreeze","fuelFreeze","crashOverride","killOverride","killRemoval","enableNetOutput","dataLogTime","testRollRate","testPitchRate","testYawRate","testBodyAxis","useCoordSys"],"own":38,"wp":[0],"po":{"0":"Player"},"d":false,"ov":["copyData","deleteData","shutdownNotification","reset","updateTC","updateData","dynamics","updateSystemPointers","processComponents"],"hd":"include/mixr/models/player/Player.hpp","src":"src/models/player/Player.cpp","ml":{"copyData":273,"deleteData":406,"shutdownNotification":433,"reset":455,"updateTC":528,"updateData":619,"dynamics":2764,"updateSystemPointers":3141,"processComponents":3163}},"LifeForm":{"b":"Player","f":null,"m":"models","r":true,"ch":["LifeForm","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"Player"},"d":false,"ov":["copyData","deleteData","shutdownNotification","reset"],"hd":"include/mixr/models/player/LifeForm.hpp","src":"src/models/player/LifeForm.cpp","ml":{"copyData":34,"deleteData":45,"shutdownNotification":54,"reset":75}},"Ship":{"b":"Player","f":null,"m":"models","r":true,"ch":["Ship","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"Player"},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/models/player/Ship.hpp","src":"src/models/player/Ship.cpp","ml":{"copyData":21,"deleteData":26}},"Building":{"b":"Player","f":null,"m":"models","r":true,"ch":["Building","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"Player"},"d":false,"ov":["copyData"],"hd":"include/mixr/models/player/Building.hpp","src":"src/models/player/Building.cpp","ml":{"copyData":21}},"SamVehicle":{"b":"GroundVehicle","f":null,"m":"models","r":true,"ch":["SamVehicle","GroundVehicle","Player","AbstractPlayer","Component"],"sl":["minLaunchRange","maxLaunchRange"],"own":2,"wp":[0],"po":{"0":"GroundVehicle"},"d":false,"ov":["copyData","updateData"],"hd":"include/mixr/models/player/ground/SamVehicle.hpp","src":"src/models/player/ground/SamVehicle.cpp","ml":{"copyData":46,"updateData":58}},"Tank":{"b":"GroundVehicle","f":null,"m":"models","r":true,"ch":["Tank","GroundVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"GroundVehicle"},"d":false,"ov":[],"hd":"include/mixr/models/player/ground/Tank.hpp","src":"src/models/player/ground/Tank.cpp","ml":{}},"GroundStation":{"b":"GroundVehicle","f":null,"m":"models","r":true,"ch":["GroundStation","GroundVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"GroundVehicle"},"d":false,"ov":[],"hd":"include/mixr/models/player/ground/GroundStation.hpp","src":"src/models/player/ground/GroundStation.cpp","ml":{}},"GroundStationUav":{"b":"GroundStation","f":null,"m":"models","r":true,"ch":["GroundStationUav","GroundStation","GroundVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"GroundVehicle"},"d":false,"ov":[],"hd":"include/mixr/models/player/ground/GroundStationUav.hpp","src":"src/models/player/ground/GroundStationUav.cpp","ml":{}},"Artillery":{"b":"GroundVehicle","f":null,"m":"models","r":true,"ch":["Artillery","GroundVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"GroundVehicle"},"d":false,"ov":[],"hd":"include/mixr/models/player/ground/Artillery.hpp","src":"src/models/player/ground/Artillery.cpp","ml":{}},"GroundStationRadar":{"b":"GroundStation","f":null,"m":"models","r":true,"ch":["GroundStationRadar","GroundStation","GroundVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"GroundVehicle"},"d":false,"ov":[],"hd":"include/mixr/models/player/ground/GroundStationRadar.hpp","src":"src/models/player/ground/GroundStationRadar.cpp","ml":{}},"WheeledVehicle":{"b":"GroundVehicle","f":null,"m":"models","r":true,"ch":["WheeledVehicle","GroundVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"GroundVehicle"},"d":false,"ov":[],"hd":"include/mixr/models/player/ground/WheeledVehicle.hpp","src":"src/models/player/ground/WheeledVehicle.cpp","ml":{}},"ArmoredVehicle":{"b":"GroundVehicle","f":null,"m":"models","r":true,"ch":["ArmoredVehicle","GroundVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"GroundVehicle"},"d":false,"ov":[],"hd":"include/mixr/models/player/ground/ArmoredVehicle.hpp","src":"src/models/player/ground/ArmoredVehicle.cpp","ml":{}},"Sam":{"b":"Missile","f":null,"m":"models","r":true,"ch":["Sam","Missile","AbstractWeapon","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"AbstractWeapon"},"d":false,"ov":[],"hd":"include/mixr/models/player/weapon/Sam.hpp","src":"src/models/player/weapon/Sam.cpp","ml":{}},"Bullet":{"b":"AbstractWeapon","f":null,"m":"models","r":true,"ch":["Bullet","AbstractWeapon","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"AbstractWeapon"},"d":false,"ov":["copyData","deleteData","shutdownNotification","reset"],"hd":"include/mixr/models/player/weapon/Bullet.hpp","src":"src/models/player/weapon/Bullet.cpp","ml":{"copyData":34,"deleteData":51,"shutdownNotification":56,"reset":62}},"Agm":{"b":"Missile","f":"AgmMissile","m":"models","r":true,"ch":["Agm","Missile","AbstractWeapon","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"AbstractWeapon"},"d":false,"ov":[],"hd":"include/mixr/models/player/weapon/Agm.hpp","src":"src/models/player/weapon/Agm.cpp","ml":{}},"Aam":{"b":"Missile","f":"AamMissile","m":"models","r":true,"ch":["Aam","Missile","AbstractWeapon","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"AbstractWeapon"},"d":false,"ov":[],"hd":"include/mixr/models/player/weapon/Aam.hpp","src":"src/models/player/weapon/Aam.cpp","ml":{}},"Missile":{"b":"AbstractWeapon","f":null,"m":"models","r":true,"ch":["Missile","AbstractWeapon","Player","AbstractPlayer","Component"],"sl":["minSpeed","maxSpeed","speedMaxG","maxg","maxAccel","cmdPitch","cmdHeading","cmdSpeed"],"own":8,"wp":[0],"po":{"0":"AbstractWeapon"},"d":false,"ov":["copyData","deleteData","reset"],"hd":"include/mixr/models/player/weapon/Missile.hpp","src":"src/models/player/weapon/Missile.cpp","ml":{"copyData":72,"deleteData":90,"reset":97}},"Bomb":{"b":"AbstractWeapon","f":null,"m":"models","r":true,"ch":["Bomb","AbstractWeapon","Player","AbstractPlayer","Component"],"sl":["arming","noseFuze","midFuze","tailFuze","fuzeAltitude","fuzeTime","dragIndex"],"own":7,"wp":[0],"po":{"0":"AbstractWeapon"},"d":false,"ov":["copyData"],"hd":"include/mixr/models/player/weapon/Bomb.hpp","src":"src/models/player/weapon/Bomb.cpp","ml":{"copyData":62}},"AbstractWeapon":{"b":"Player","f":null,"m":"models","r":false,"ch":["AbstractWeapon","Player","AbstractPlayer","Component"],"sl":["released","failed","power","hang","hung","maxTOF","tsg","maxBurstRng","lethalRange","sobt","eobt","maxGimbal","tgtPos","weaponID","dummy","jettisonable","testTgtName"],"own":17,"wp":[0],"po":{"0":"AbstractWeapon"},"d":false,"ov":["copyData","deleteData","reset","updateTC","dynamics","shutdownNotification"],"hd":"include/mixr/models/player/weapon/AbstractWeapon.hpp","src":"src/models/player/weapon/AbstractWeapon.cpp","ml":{"copyData":115,"deleteData":161,"reset":174,"updateTC":225,"dynamics":253,"shutdownNotification":302}},"Helicopter":{"b":"AirVehicle","f":null,"m":"models","r":true,"ch":["Helicopter","AirVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"Player"},"d":false,"ov":[],"hd":"include/mixr/models/player/air/Helicopter.hpp","src":"src/models/player/air/Helicopter.cpp","ml":{}},"AirVehicle":{"b":"Player","f":null,"m":"models","r":true,"ch":["AirVehicle","Player","AbstractPlayer","Component"],"sl":["initGearPos","up","down"],"own":3,"wp":[0],"po":{"0":"Player"},"d":false,"ov":["copyData","reset"],"hd":"include/mixr/models/player/air/AirVehicle.hpp","src":"src/models/player/air/AirVehicle.cpp","ml":{"copyData":38,"reset":59}},"UnmannedAirVehicle":{"b":"AirVehicle","f":null,"m":"models","r":true,"ch":["UnmannedAirVehicle","AirVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"Player"},"d":false,"ov":[],"hd":"include/mixr/models/player/air/UnmannedAirVehicle.hpp","src":"src/models/player/air/UnmannedAirVehicle.cpp","ml":{}},"BoosterSpaceVehicle":{"b":"SpaceVehicle","f":null,"m":"models","r":true,"ch":["BoosterSpaceVehicle","SpaceVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"Player"},"d":false,"ov":[],"hd":"include/mixr/models/player/space/BoosterSpaceVehicle.hpp","src":"src/models/player/space/BoosterSpaceVehicle.cpp","ml":{}},"MannedSpaceVehicle":{"b":"SpaceVehicle","f":null,"m":"models","r":true,"ch":["MannedSpaceVehicle","SpaceVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"Player"},"d":false,"ov":[],"hd":"include/mixr/models/player/space/MannedSpaceVehicle.hpp","src":"src/models/player/space/MannedSpaceVehicle.cpp","ml":{}},"SpaceVehicle":{"b":"Player","f":null,"m":"models","r":true,"ch":["SpaceVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"Player"},"d":false,"ov":[],"hd":"include/mixr/models/player/space/SpaceVehicle.hpp","src":"src/models/player/space/SpaceVehicle.cpp","ml":{}},"UnmannedSpaceVehicle":{"b":"SpaceVehicle","f":null,"m":"models","r":true,"ch":["UnmannedSpaceVehicle","SpaceVehicle","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"UnmannedSpaceVehicle"},"d":false,"ov":["dynamics"],"hd":"include/mixr/models/player/space/UnmannedSpaceVehicle.hpp","src":"src/models/player/space/UnmannedSpaceVehicle.cpp","ml":{"dynamics":19}},"Chaff":{"b":"Effect","f":null,"m":"models","r":true,"ch":["Chaff","Effect","AbstractWeapon","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"AbstractWeapon"},"d":false,"ov":[],"hd":"include/mixr/models/player/effect/Chaff.hpp","src":"src/models/player/effect/Chaff.cpp","ml":{}},"Effect":{"b":"AbstractWeapon","f":null,"m":"models","r":false,"ch":["Effect","AbstractWeapon","Player","AbstractPlayer","Component"],"sl":["dragIndex"],"own":1,"wp":[0],"po":{"0":"AbstractWeapon"},"d":false,"ov":["copyData"],"hd":"include/mixr/models/player/effect/Effect.hpp","src":"src/models/player/effect/Effect.cpp","ml":{"copyData":46}},"Decoy":{"b":"Effect","f":null,"m":"models","r":true,"ch":["Decoy","Effect","AbstractWeapon","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"AbstractWeapon"},"d":false,"ov":[],"hd":"include/mixr/models/player/effect/Decoy.hpp","src":"src/models/player/effect/Decoy.cpp","ml":{}},"Flare":{"b":"Effect","f":null,"m":"models","r":true,"ch":["Flare","Effect","AbstractWeapon","Player","AbstractPlayer","Component"],"sl":[],"own":0,"wp":[0],"po":{"0":"AbstractWeapon"},"d":false,"ov":[],"hd":"include/mixr/models/player/effect/Flare.hpp","src":"src/models/player/effect/Flare.cpp","ml":{}},"String":{"b":"Object","f":null,"m":"base","r":false,"ch":["String"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/String.hpp","src":"src/base/String.cpp","ml":{"copyData":31,"deleteData":41}},"Pair":{"b":"Object","f":null,"m":"base","r":false,"ch":["Pair"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/Pair.hpp","src":"src/base/Pair.cpp","ml":{"copyData":28,"deleteData":58}},"ThisType":{"b":"BaseType","f":null,"m":"base","r":false,"ch":["ThisType"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/macros.hpp","src":null,"ml":{}},"Component":{"b":"Object","f":null,"m":"base","r":false,"ch":["Component"],"sl":["components","select","enableTimingStats","printTimingStats","freeze","enableMessageType","disableMessageType"],"own":7,"wp":[],"po":{},"d":false,"ov":["event","copyData","deleteData","reset","tcFrame","updateTC","updateData","shutdownNotification","processComponents"],"hd":"include/mixr/base/Component.hpp","src":"src/base/Component.cpp","ml":{"event":45,"copyData":70,"deleteData":103,"reset":159,"tcFrame":184,"updateTC":243,"updateData":269,"shutdownNotification":333,"processComponents":583}},"MonitorMetrics":{"b":"Object","f":"monitorMetrics","m":"base","r":false,"ch":["MonitorMetrics"],"sl":["red","green","blue","phosphors","whiteRGB","whiteCIE"],"own":6,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/MonitorMetrics.hpp","src":"src/base/MonitorMetrics.cpp","ml":{"copyData":80,"deleteData":99}},"Transform":{"b":"Object","f":null,"m":"base","r":false,"ch":["Transform"],"sl":["x","y","z","w"],"own":4,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/Transforms.hpp","src":"src/base/Transforms.cpp","ml":{"copyData":33,"deleteData":43}},"Translation":{"b":"Transform","f":null,"m":"base","r":true,"ch":["Translation","Transform"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/Transforms.hpp","src":"src/base/Transforms.cpp","ml":{}},"Rotation":{"b":"Transform","f":null,"m":"base","r":true,"ch":["Rotation","Transform"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/Transforms.hpp","src":"src/base/Transforms.cpp","ml":{}},"Scale":{"b":"Transform","f":null,"m":"base","r":true,"ch":["Scale","Transform"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/Transforms.hpp","src":"src/base/Transforms.cpp","ml":{}},"Identifier":{"b":"String","f":null,"m":"base","r":false,"ch":["Identifier","String"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/Identifier.hpp","src":"src/base/Identifier.cpp","ml":{}},"List":{"b":"Object","f":null,"m":"base","r":false,"ch":["List"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/List.hpp","src":"src/base/List.cpp","ml":{"copyData":45,"deleteData":63}},"Stack":{"b":"List","f":null,"m":"base","r":false,"ch":["Stack","List"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/Stack.hpp","src":"src/base/Stack.cpp","ml":{}},"Statistic":{"b":"Object","f":null,"m":"base","r":true,"ch":["Statistic"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/Statistic.hpp","src":"src/base/Statistic.cpp","ml":{"copyData":20}},"Matrix":{"b":"Object","f":null,"m":"base","r":false,"ch":["Matrix"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/Matrix.hpp","src":"src/base/Matrix.cpp","ml":{"copyData":46,"deleteData":62}},"PairStream":{"b":"List","f":null,"m":"base","r":false,"ch":["PairStream","List"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/PairStream.hpp","src":"src/base/PairStream.cpp","ml":{}},"Locus":{"b":"Object","f":null,"m":"base","r":false,"ch":["Locus"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/Locus.hpp","src":"src/base/Locus.cpp","ml":{"copyData":30,"deleteData":46}},"LatLon":{"b":"Number","f":null,"m":"base","r":true,"ch":["LatLon","Number"],"sl":["direction","n","s","e","w","degrees","minutes","seconds"],"own":8,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/LatLon.hpp","src":"src/base/LatLon.cpp","ml":{"copyData":34}},"EarthModel":{"b":"Object","f":null,"m":"base","r":true,"ch":["EarthModel"],"sl":["a","b","f"],"own":3,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/EarthModel.hpp","src":"src/base/EarthModel.cpp","ml":{"copyData":80}},"StateMachine":{"b":"Component","f":"AbstractStateMachine","m":"base","r":false,"ch":["StateMachine","Component"],"sl":["stateMachines"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","reset","updateData","updateTC"],"hd":"include/mixr/base/StateMachine.hpp","src":"src/base/StateMachine.cpp","ml":{"copyData":51,"deleteData":59,"reset":68,"updateData":96,"updateTC":105}},"RVector":{"b":"Matrix","f":null,"m":"base","r":false,"ch":["RVector","Matrix"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/Vectors.hpp","src":"src/base/Vectors.cpp","ml":{}},"CVector":{"b":"Matrix","f":null,"m":"base","r":false,"ch":["CVector","Matrix"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/Vectors.hpp","src":"src/base/Vectors.cpp","ml":{}},"Timer":{"b":"Object","f":null,"m":"base","r":false,"ch":["Timer"],"sl":["timerValue","alarmTime","active"],"own":3,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","reset"],"hd":"include/mixr/base/Timers.hpp","src":"src/base/Timers.cpp","ml":{"copyData":46,"deleteData":60,"reset":71}},"UpTimer":{"b":"Timer","f":null,"m":"base","r":true,"ch":["UpTimer","Timer"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/Timers.hpp","src":"src/base/Timers.cpp","ml":{}},"DownTimer":{"b":"Timer","f":null,"m":"base","r":true,"ch":["DownTimer","Timer"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/Timers.hpp","src":"src/base/Timers.cpp","ml":{}},"Arbiter":{"b":"AbstractBehavior","f":"UbfArbiter","m":"base","r":true,"ch":["Arbiter","AbstractBehavior","Component"],"sl":["behaviors"],"own":1,"wp":[],"po":{},"d":false,"ov":["deleteData"],"hd":"include/mixr/base/ubf/Arbiter.hpp","src":"src/base/ubf/Arbiter.cpp","ml":{"deleteData":29}},"Agent":{"b":"Component","f":"UbfAgent","m":"base","r":true,"ch":["Agent","Component"],"sl":["state","behavior"],"own":2,"wp":[],"po":{},"d":false,"ov":["deleteData","reset","updateData"],"hd":"include/mixr/base/ubf/Agent.hpp","src":"src/base/ubf/Agent.cpp","ml":{"deleteData":32,"reset":40,"updateData":59}},"AgentTC":{"b":"Agent","f":"UbfAgentTC","m":"base","r":false,"ch":["AgentTC","Agent","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["updateTC"],"hd":"include/mixr/base/ubf/Agent.hpp","src":"src/base/ubf/Agent.cpp","ml":{"updateTC":170}},"AbstractState":{"b":"Component","f":null,"m":"base","r":false,"ch":["AbstractState","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/ubf/AbstractState.hpp","src":"src/base/ubf/AbstractState.cpp","ml":{}},"AbstractBehavior":{"b":"Component","f":null,"m":"base","r":false,"ch":["AbstractBehavior","Component"],"sl":["vote"],"own":1,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/ubf/AbstractBehavior.hpp","src":"src/base/ubf/AbstractBehavior.cpp","ml":{}},"AbstractAction":{"b":"Object","f":null,"m":"base","r":false,"ch":["AbstractAction"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/ubf/AbstractAction.hpp","src":"src/base/ubf/AbstractAction.cpp","ml":{"copyData":18}},"Mass":{"b":"Number","f":"AbstractMass","m":"base","r":false,"ch":["Mass","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/Masses.hpp","src":"src/base/units/Masses.cpp","ml":{"copyData":26}},"KiloGrams":{"b":"Mass","f":null,"m":"base","r":true,"ch":["KiloGrams","Mass","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Masses.hpp","src":"src/base/units/Masses.cpp","ml":{}},"Grams":{"b":"Mass","f":null,"m":"base","r":true,"ch":["Grams","Mass","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Masses.hpp","src":"src/base/units/Masses.cpp","ml":{}},"Slugs":{"b":"Mass","f":null,"m":"base","r":true,"ch":["Slugs","Mass","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Masses.hpp","src":"src/base/units/Masses.cpp","ml":{}},"Power":{"b":"Number","f":null,"m":"base","r":false,"ch":["Power","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/Powers.hpp","src":"src/base/units/Powers.cpp","ml":{"copyData":25}},"Watts":{"b":"Power","f":null,"m":"base","r":true,"ch":["Watts","Power","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Powers.hpp","src":"src/base/units/Powers.cpp","ml":{}},"MilliWatts":{"b":"Power","f":null,"m":"base","r":true,"ch":["MilliWatts","Power","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Powers.hpp","src":"src/base/units/Powers.cpp","ml":{}},"KiloWatts":{"b":"Power","f":null,"m":"base","r":true,"ch":["KiloWatts","Power","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Powers.hpp","src":"src/base/units/Powers.cpp","ml":{}},"Horsepower":{"b":"Power","f":null,"m":"base","r":true,"ch":["Horsepower","Power","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Powers.hpp","src":"src/base/units/Powers.cpp","ml":{}},"DecibelWatts":{"b":"Power","f":null,"m":"base","r":true,"ch":["DecibelWatts","Power","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Powers.hpp","src":"src/base/units/Powers.cpp","ml":{}},"DecibelMilliWatts":{"b":"Power","f":null,"m":"base","r":true,"ch":["DecibelMilliWatts","Power","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Powers.hpp","src":"src/base/units/Powers.cpp","ml":{}},"FlowRate":{"b":"Number","f":null,"m":"base","r":false,"ch":["FlowRate","Number"],"sl":["volume","flowTime"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/units/FlowRate.hpp","src":"src/base/units/FlowRate.cpp","ml":{"copyData":56,"deleteData":69}},"Force":{"b":"Number","f":"AbstractForce","m":"base","r":false,"ch":["Force","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/Forces.hpp","src":"src/base/units/Forces.cpp","ml":{"copyData":26}},"Newtons":{"b":"Force","f":null,"m":"base","r":true,"ch":["Newtons","Force","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Forces.hpp","src":"src/base/units/Forces.cpp","ml":{}},"KiloNewtons":{"b":"Force","f":null,"m":"base","r":true,"ch":["KiloNewtons","Force","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Forces.hpp","src":"src/base/units/Forces.cpp","ml":{}},"PoundForces":{"b":"Force","f":null,"m":"base","r":true,"ch":["PoundForces","Force","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Forces.hpp","src":"src/base/units/Forces.cpp","ml":{}},"Poundals":{"b":"Force","f":null,"m":"base","r":true,"ch":["Poundals","Force","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Forces.hpp","src":"src/base/units/Forces.cpp","ml":{}},"Angle":{"b":"Number","f":"AbstractAngle","m":"base","r":false,"ch":["Angle","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/Angles.hpp","src":"src/base/units/Angles.cpp","ml":{"copyData":26}},"Degrees":{"b":"Angle","f":null,"m":"base","r":true,"ch":["Degrees","Angle","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Angles.hpp","src":"src/base/units/Angles.cpp","ml":{}},"Radians":{"b":"Angle","f":null,"m":"base","r":true,"ch":["Radians","Angle","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Angles.hpp","src":"src/base/units/Angles.cpp","ml":{}},"Semicircles":{"b":"Angle","f":null,"m":"base","r":true,"ch":["Semicircles","Angle","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Angles.hpp","src":"src/base/units/Angles.cpp","ml":{}},"LinearVelocity":{"b":"Number","f":null,"m":"base","r":true,"ch":["LinearVelocity","Number"],"sl":["distance","time"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/LinearVelocity.hpp","src":"src/base/units/LinearVelocity.cpp","ml":{"copyData":75}},"Decibel":{"b":"Number","f":"dB","m":"base","r":true,"ch":["Decibel","Number"],"sl":["value"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/Decibel.hpp","src":"src/base/units/Decibel.cpp","ml":{"copyData":32}},"Time":{"b":"Number","f":"AbstractTime","m":"base","r":false,"ch":["Time","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/Times.hpp","src":"src/base/units/Times.cpp","ml":{"copyData":27}},"Seconds":{"b":"Time","f":null,"m":"base","r":true,"ch":["Seconds","Time","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Times.hpp","src":"src/base/units/Times.cpp","ml":{}},"MilliSeconds":{"b":"Time","f":null,"m":"base","r":true,"ch":["MilliSeconds","Time","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Times.hpp","src":"src/base/units/Times.cpp","ml":{}},"MicroSeconds":{"b":"Time","f":null,"m":"base","r":true,"ch":["MicroSeconds","Time","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Times.hpp","src":"src/base/units/Times.cpp","ml":{}},"NanoSeconds":{"b":"Time","f":null,"m":"base","r":true,"ch":["NanoSeconds","Time","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Times.hpp","src":"src/base/units/Times.cpp","ml":{}},"Minutes":{"b":"Time","f":null,"m":"base","r":true,"ch":["Minutes","Time","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Times.hpp","src":"src/base/units/Times.cpp","ml":{}},"Hours":{"b":"Time","f":null,"m":"base","r":true,"ch":["Hours","Time","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Times.hpp","src":"src/base/units/Times.cpp","ml":{}},"Days":{"b":"Time","f":null,"m":"base","r":true,"ch":["Days","Time","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Times.hpp","src":"src/base/units/Times.cpp","ml":{}},"Area":{"b":"Number","f":"AbstractArea","m":"base","r":false,"ch":["Area","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/Areas.hpp","src":"src/base/units/Areas.cpp","ml":{"copyData":32}},"SquareMeters":{"b":"Area","f":null,"m":"base","r":true,"ch":["SquareMeters","Area","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Areas.hpp","src":"src/base/units/Areas.cpp","ml":{}},"SquareFeet":{"b":"Area","f":null,"m":"base","r":true,"ch":["SquareFeet","Area","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Areas.hpp","src":"src/base/units/Areas.cpp","ml":{}},"SquareInches":{"b":"Area","f":null,"m":"base","r":true,"ch":["SquareInches","Area","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Areas.hpp","src":"src/base/units/Areas.cpp","ml":{}},"SquareYards":{"b":"Area","f":null,"m":"base","r":true,"ch":["SquareYards","Area","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Areas.hpp","src":"src/base/units/Areas.cpp","ml":{}},"SquareMiles":{"b":"Area","f":null,"m":"base","r":true,"ch":["SquareMiles","Area","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Areas.hpp","src":"src/base/units/Areas.cpp","ml":{}},"SquareCentiMeters":{"b":"Area","f":null,"m":"base","r":true,"ch":["SquareCentiMeters","Area","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Areas.hpp","src":"src/base/units/Areas.cpp","ml":{}},"SquareMilliMeters":{"b":"Area","f":null,"m":"base","r":true,"ch":["SquareMilliMeters","Area","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Areas.hpp","src":"src/base/units/Areas.cpp","ml":{}},"SquareKiloMeters":{"b":"Area","f":null,"m":"base","r":true,"ch":["SquareKiloMeters","Area","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Areas.hpp","src":"src/base/units/Areas.cpp","ml":{}},"DecibelSquareMeters":{"b":"Area","f":null,"m":"base","r":true,"ch":["DecibelSquareMeters","Area","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Areas.hpp","src":"src/base/units/Areas.cpp","ml":{}},"Distance":{"b":"Number","f":"AbstractDistance","m":"base","r":false,"ch":["Distance","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/Distances.hpp","src":"src/base/units/Distances.cpp","ml":{"copyData":28}},"Meters":{"b":"Distance","f":null,"m":"base","r":true,"ch":["Meters","Distance","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Distances.hpp","src":"src/base/units/Distances.cpp","ml":{}},"CentiMeters":{"b":"Distance","f":null,"m":"base","r":true,"ch":["CentiMeters","Distance","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Distances.hpp","src":"src/base/units/Distances.cpp","ml":{}},"MicroMeters":{"b":"Distance","f":null,"m":"base","r":true,"ch":["MicroMeters","Distance","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Distances.hpp","src":"src/base/units/Distances.cpp","ml":{}},"Microns":{"b":"Distance","f":null,"m":"base","r":true,"ch":["Microns","Distance","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Distances.hpp","src":"src/base/units/Distances.cpp","ml":{}},"KiloMeters":{"b":"Distance","f":null,"m":"base","r":true,"ch":["KiloMeters","Distance","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Distances.hpp","src":"src/base/units/Distances.cpp","ml":{}},"Inches":{"b":"Distance","f":null,"m":"base","r":true,"ch":["Inches","Distance","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Distances.hpp","src":"src/base/units/Distances.cpp","ml":{}},"Feet":{"b":"Distance","f":null,"m":"base","r":true,"ch":["Feet","Distance","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Distances.hpp","src":"src/base/units/Distances.cpp","ml":{}},"NauticalMiles":{"b":"Distance","f":null,"m":"base","r":true,"ch":["NauticalMiles","Distance","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Distances.hpp","src":"src/base/units/Distances.cpp","ml":{}},"StatuteMiles":{"b":"Distance","f":null,"m":"base","r":true,"ch":["StatuteMiles","Distance","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Distances.hpp","src":"src/base/units/Distances.cpp","ml":{}},"Density":{"b":"Number","f":null,"m":"base","r":false,"ch":["Density","Number"],"sl":["mass","volume"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/Density.hpp","src":"src/base/units/Density.cpp","ml":{"copyData":53}},"Frequency":{"b":"Number","f":"AbstractFrequency","m":"base","r":false,"ch":["Frequency","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/Frequencies.hpp","src":"src/base/units/Frequencies.cpp","ml":{"copyData":28}},"Hertz":{"b":"Frequency","f":null,"m":"base","r":true,"ch":["Hertz","Frequency","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Frequencies.hpp","src":"src/base/units/Frequencies.cpp","ml":{}},"KiloHertz":{"b":"Frequency","f":null,"m":"base","r":true,"ch":["KiloHertz","Frequency","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Frequencies.hpp","src":"src/base/units/Frequencies.cpp","ml":{}},"MegaHertz":{"b":"Frequency","f":null,"m":"base","r":true,"ch":["MegaHertz","Frequency","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Frequencies.hpp","src":"src/base/units/Frequencies.cpp","ml":{}},"GigaHertz":{"b":"Frequency","f":null,"m":"base","r":true,"ch":["GigaHertz","Frequency","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Frequencies.hpp","src":"src/base/units/Frequencies.cpp","ml":{}},"TeraHertz":{"b":"Frequency","f":null,"m":"base","r":true,"ch":["TeraHertz","Frequency","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Frequencies.hpp","src":"src/base/units/Frequencies.cpp","ml":{}},"AngularVelocity":{"b":"Number","f":null,"m":"base","r":true,"ch":["AngularVelocity","Number"],"sl":["angle","time"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/AngularVelocity.hpp","src":"src/base/units/AngularVelocity.cpp","ml":{"copyData":221}},"Volume":{"b":"Number","f":"AbstractVolume","m":"base","r":false,"ch":["Volume","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/Volumes.hpp","src":"src/base/units/Volumes.cpp","ml":{"copyData":25}},"CubicMeters":{"b":"Volume","f":null,"m":"base","r":false,"ch":["CubicMeters","Volume","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Volumes.hpp","src":"src/base/units/Volumes.cpp","ml":{}},"CubicFeet":{"b":"Volume","f":null,"m":"base","r":false,"ch":["CubicFeet","Volume","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Volumes.hpp","src":"src/base/units/Volumes.cpp","ml":{}},"CubicInches":{"b":"Volume","f":null,"m":"base","r":false,"ch":["CubicInches","Volume","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Volumes.hpp","src":"src/base/units/Volumes.cpp","ml":{}},"Liters":{"b":"Volume","f":null,"m":"base","r":false,"ch":["Liters","Volume","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Volumes.hpp","src":"src/base/units/Volumes.cpp","ml":{}},"Energy":{"b":"Number","f":"AbstractEnergy","m":"base","r":false,"ch":["Energy","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/units/Energies.hpp","src":"src/base/units/Energies.cpp","ml":{"copyData":26}},"Joules":{"b":"Energy","f":null,"m":"base","r":true,"ch":["Joules","Energy","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Energies.hpp","src":"src/base/units/Energies.cpp","ml":{}},"KiloWattHours":{"b":"Energy","f":null,"m":"base","r":true,"ch":["KiloWattHours","Energy","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Energies.hpp","src":"src/base/units/Energies.cpp","ml":{}},"BTUs":{"b":"Energy","f":null,"m":"base","r":true,"ch":["BTUs","Energy","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Energies.hpp","src":"src/base/units/Energies.cpp","ml":{}},"Calories":{"b":"Energy","f":null,"m":"base","r":true,"ch":["Calories","Energy","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Energies.hpp","src":"src/base/units/Energies.cpp","ml":{}},"FootPounds":{"b":"Energy","f":null,"m":"base","r":true,"ch":["FootPounds","Energy","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/units/Energies.hpp","src":"src/base/units/Energies.cpp","ml":{}},"TcpServerSingle":{"b":"TcpHandler","f":null,"m":"base","r":true,"ch":["TcpServerSingle","TcpHandler","PosixHandler","NetHandler","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/network/TcpServerSingle.hpp","src":"src/base/network/TcpServerSingle.cpp","ml":{}},"NetHandler":{"b":"Component","f":null,"m":"base","r":false,"ch":["NetHandler","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/network/NetHandler.hpp","src":"src/base/network/NetHandler.cpp","ml":{}},"UdpBroadcastHandler":{"b":"PosixHandler","f":null,"m":"base","r":true,"ch":["UdpBroadcastHandler","PosixHandler","NetHandler","Component"],"sl":["networkMask","255.255.255.255"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/network/UdpBroadcastHandler.hpp","src":"src/base/network/UdpBroadcastHandler.cpp","ml":{"copyData":56,"deleteData":69}},"PosixHandler":{"b":"NetHandler","f":null,"m":"base","r":false,"ch":["PosixHandler","NetHandler","Component"],"sl":["localIpAddress",".","localPort","port","shared","sendBuffSizeKb","recvBuffSizeKb","ignoreSourcePort"],"own":8,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/network/PosixHandler.hpp","src":"src/base/network/PosixHandler.cpp","ml":{"copyData":68,"deleteData":91}},"UdpMulticastHandler":{"b":"PosixHandler","f":null,"m":"base","r":true,"ch":["UdpMulticastHandler","PosixHandler","NetHandler","Component"],"sl":["multicastGroup",".","225.0.0.251","ttl","loopback"],"own":5,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/network/UdpMulticastHandler.hpp","src":"src/base/network/UdpMulticastHandler.cpp","ml":{"copyData":68,"deleteData":83}},"TcpClient":{"b":"TcpHandler","f":null,"m":"base","r":true,"ch":["TcpClient","TcpHandler","PosixHandler","NetHandler","Component"],"sl":["ipAddress","."],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/network/TcpClient.hpp","src":"src/base/network/TcpClient.cpp","ml":{"copyData":59,"deleteData":75}},"TcpHandler":{"b":"PosixHandler","f":null,"m":"base","r":false,"ch":["TcpHandler","PosixHandler","NetHandler","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/network/TcpHandler.hpp","src":"src/base/network/TcpHandler.cpp","ml":{"copyData":59}},"TcpServerMultiple":{"b":"TcpHandler","f":null,"m":"base","r":true,"ch":["TcpServerMultiple","TcpHandler","PosixHandler","NetHandler","Component"],"sl":["backlog"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/network/TcpServerMultiple.hpp","src":"src/base/network/TcpServerMultiple.cpp","ml":{"copyData":58}},"UdpUnicastHandler":{"b":"PosixHandler","f":null,"m":"base","r":true,"ch":["UdpUnicastHandler","PosixHandler","NetHandler","Component"],"sl":["ipAddress","."],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/network/UdpUnicastHandler.hpp","src":"src/base/network/UdpUnicastHandler.cpp","ml":{"copyData":56,"deleteData":70}},"AbstractIoData":{"b":"Object","f":null,"m":"base","r":false,"ch":["AbstractIoData"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/concepts/linkage/AbstractIoData.hpp","src":"src/base/concepts/linkage/AbstractIoData.cpp","ml":{}},"AbstractIoHandler":{"b":"Component","f":null,"m":"base","r":false,"ch":["AbstractIoHandler","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/concepts/linkage/AbstractIoHandler.hpp","src":"src/base/concepts/linkage/AbstractIoHandler.cpp","ml":{}},"AbstractIoDevice":{"b":"Object","f":null,"m":"base","r":false,"ch":["AbstractIoDevice"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/concepts/linkage/AbstractIoDevice.hpp","src":"src/base/concepts/linkage/AbstractIoDevice.cpp","ml":{}},"Func5":{"b":"Function","f":null,"m":"base","r":true,"ch":["Func5","Function"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/functors/Func5.hpp","src":"src/base/functors/Func5.cpp","ml":{}},"Func2":{"b":"Function","f":null,"m":"base","r":true,"ch":["Func2","Function"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/functors/Func2.hpp","src":"src/base/functors/Func2.cpp","ml":{}},"Func3":{"b":"Function","f":null,"m":"base","r":true,"ch":["Func3","Function"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/functors/Func3.hpp","src":"src/base/functors/Func3.cpp","ml":{}},"Table5":{"b":"Table4","f":null,"m":"base","r":true,"ch":["Table5","Table4","Table3","Table2","Table1","Table"],"sl":["v"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/functors/Table5.hpp","src":"src/base/functors/Table5.cpp","ml":{"copyData":49,"deleteData":66}},"Table4":{"b":"Table3","f":null,"m":"base","r":true,"ch":["Table4","Table3","Table2","Table1","Table"],"sl":["w"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/functors/Table4.hpp","src":"src/base/functors/Table4.cpp","ml":{"copyData":45,"deleteData":62}},"TableStorage":{"b":"FStorage","f":null,"m":"base","r":false,"ch":["TableStorage","FStorage"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/functors/TableStorage.hpp","src":"src/base/functors/TableStorage.cpp","ml":{"copyData":16}},"Func4":{"b":"Function","f":null,"m":"base","r":true,"ch":["Func4","Function"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/functors/Func4.hpp","src":"src/base/functors/Func4.cpp","ml":{}},"Function":{"b":"Object","f":null,"m":"base","r":false,"ch":["Function"],"sl":["table"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/functors/Function.hpp","src":"src/base/functors/Function.cpp","ml":{"copyData":30,"deleteData":45}},"Table3":{"b":"Table2","f":null,"m":"base","r":true,"ch":["Table3","Table2","Table1","Table"],"sl":["z"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/functors/Table3.hpp","src":"src/base/functors/Table3.cpp","ml":{"copyData":44,"deleteData":61}},"Func1":{"b":"Function","f":null,"m":"base","r":true,"ch":["Func1","Function"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/functors/Func1.hpp","src":"src/base/functors/Func1.cpp","ml":{}},"Table":{"b":"Object","f":null,"m":"base","r":false,"ch":["Table"],"sl":["data","extrapolate"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/functors/Table.hpp","src":"src/base/functors/Table.cpp","ml":{"copyData":70,"deleteData":94}},"Table2":{"b":"Table1","f":null,"m":"base","r":true,"ch":["Table2","Table1","Table"],"sl":["y"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/functors/Table2.hpp","src":"src/base/functors/Table2.cpp","ml":{"copyData":45,"deleteData":62}},"FStorage":{"b":"Object","f":null,"m":"base","r":false,"ch":["FStorage"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/functors/FStorage.hpp","src":"src/base/functors/FStorage.cpp","ml":{}},"Polynomial":{"b":"Func1","f":null,"m":"base","r":true,"ch":["Polynomial","Func1","Function"],"sl":["coefficients"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/functors/Polynomial.hpp","src":"src/base/functors/Polynomial.cpp","ml":{"copyData":25,"deleteData":32}},"Table1":{"b":"Table","f":null,"m":"base","r":true,"ch":["Table1","Table"],"sl":["x"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/functors/Table1.hpp","src":"src/base/functors/Table1.cpp","ml":{"copyData":44,"deleteData":61}},"Yiq":{"b":"Color","f":"yiq","m":"base","r":true,"ch":["Yiq","Color"],"sl":["y","i","q"],"own":3,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/colors/Yiq.hpp","src":"src/base/colors/Yiq.cpp","ml":{"copyData":42}},"Cmy":{"b":"Color","f":"cmy","m":"base","r":true,"ch":["Cmy","Color"],"sl":["cyan","magenta","yellow"],"own":3,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/colors/Cmy.hpp","src":"src/base/colors/Cmy.cpp","ml":{"copyData":42}},"Hsv":{"b":"Color","f":"hsv","m":"base","r":true,"ch":["Hsv","Color"],"sl":["hue","saturation","value"],"own":3,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/colors/Hsv.hpp","src":"src/base/colors/Hsv.cpp","ml":{"copyData":52}},"Rgb":{"b":"Color","f":"rgb","m":"base","r":true,"ch":["Rgb","Color"],"sl":["red","green","blue"],"own":3,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/colors/Rgb.hpp","src":"src/base/colors/Rgb.cpp","ml":{"copyData":38}},"Hsva":{"b":"Hsv","f":"hsva","m":"base","r":true,"ch":["Hsva","Hsv","Color"],"sl":["alpha"],"own":1,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/colors/Hsva.hpp","src":"src/base/colors/Hsva.cpp","ml":{}},"Rgba":{"b":"Rgb","f":"rgba","m":"base","r":true,"ch":["Rgba","Rgb","Color"],"sl":["alpha"],"own":1,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/colors/Rgba.hpp","src":"src/base/colors/Rgba.cpp","ml":{}},"Color":{"b":"Object","f":null,"m":"base","r":true,"ch":["Color"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/colors/Color.hpp","src":"src/base/colors/Color.cpp","ml":{"copyData":28}},"Hls":{"b":"Color","f":"hls","m":"base","r":true,"ch":["Hls","Color"],"sl":["hue","lightness","saturation"],"own":3,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/colors/Hls.hpp","src":"src/base/colors/Hls.cpp","ml":{"copyData":46}},"Cie":{"b":"Color","f":"cie","m":"base","r":true,"ch":["Cie","Color"],"sl":["luminance","x","y","monitor"],"own":4,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/base/colors/Cie.hpp","src":"src/base/colors/Cie.cpp","ml":{"copyData":47,"deleteData":57}},"Float":{"b":"Number","f":"float","m":"base","r":true,"ch":["Float","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/numeric/Float.hpp","src":"src/base/numeric/Float.cpp","ml":{}},"Boolean":{"b":"Number","f":"boolean","m":"base","r":true,"ch":["Boolean","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/numeric/Boolean.hpp","src":"src/base/numeric/Boolean.cpp","ml":{}},"Complex":{"b":"Number","f":null,"m":"base","r":true,"ch":["Complex","Number"],"sl":["imag"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/numeric/Complex.hpp","src":"src/base/numeric/Complex.cpp","ml":{"copyData":38}},"Integer":{"b":"Number","f":"int","m":"base","r":true,"ch":["Integer","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/numeric/Integer.hpp","src":"src/base/numeric/Integer.cpp","ml":{}},"Number":{"b":"Object","f":null,"m":"base","r":true,"ch":["Number"],"sl":["value"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/numeric/Number.hpp","src":"src/base/numeric/Number.cpp","ml":{"copyData":19}},"Add":{"b":"Number","f":"+","m":"base","r":true,"ch":["Add","Number"],"sl":["n2","n3","n4","n5","n6","n7","n8","n9","n10"],"own":9,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/base/numeric/Operators.hpp","src":"src/base/numeric/Operators.cpp","ml":{"copyData":65}},"Subtract":{"b":"Add","f":"-","m":"base","r":true,"ch":["Subtract","Add","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/numeric/Operators.hpp","src":"src/base/numeric/Operators.cpp","ml":{}},"Multiply":{"b":"Add","f":"*","m":"base","r":true,"ch":["Multiply","Add","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/numeric/Operators.hpp","src":"src/base/numeric/Operators.cpp","ml":{}},"Divide":{"b":"Add","f":"/","m":"base","r":true,"ch":["Divide","Add","Number"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/base/numeric/Operators.hpp","src":"src/base/numeric/Operators.cpp","ml":{}},"DataFile":{"b":"Terrain","f":null,"m":"terrain","r":false,"ch":["DataFile","Terrain","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/terrain/DataFile.hpp","src":"src/terrain/DataFile.cpp","ml":{"copyData":19,"deleteData":48}},"Terrain":{"b":"Component","f":"AbstractTerrain","m":"terrain","r":false,"ch":["Terrain","Component"],"sl":["file","path"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","reset"],"hd":"include/mixr/terrain/Terrain.hpp","src":"src/terrain/Terrain.cpp","ml":{"copyData":39,"deleteData":66,"reset":74}},"QuadMap":{"b":"Terrain","f":null,"m":"terrain","r":true,"ch":["QuadMap","Terrain","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","reset"],"hd":"include/mixr/terrain/QuadMap.hpp","src":"src/terrain/QuadMap.cpp","ml":{"copyData":23,"deleteData":33,"reset":41}},"DedFile":{"b":"DataFile","f":null,"m":"terrain","r":true,"ch":["DedFile","DataFile","Terrain","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/terrain/ded/DedFile.hpp","src":"src/terrain/ded/DedFile.cpp","ml":{"copyData":102,"deleteData":135}},"SrtmHgtFile":{"b":"DataFile","f":null,"m":"terrain","r":true,"ch":["SrtmHgtFile","DataFile","Terrain","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/terrain/srtm/SrtmHgtFile.hpp","src":"src/terrain/srtm/SrtmHgtFile.cpp","ml":{"copyData":88}},"DtedFile":{"b":"DataFile","f":null,"m":"terrain","r":true,"ch":["DtedFile","DataFile","Terrain","Component"],"sl":["verifyChecksum"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/terrain/dted/DtedFile.hpp","src":"src/terrain/dted/DtedFile.cpp","ml":{"copyData":122}},"AbstractIgHost":{"b":"Component","f":null,"m":"simulation","r":false,"ch":["AbstractIgHost","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/simulation/AbstractIgHost.hpp","src":"src/simulation/AbstractIgHost.cpp","ml":{}},"AbstractPlayer":{"b":"Component","f":null,"m":"simulation","r":false,"ch":["AbstractPlayer","Component"],"sl":["id","mode"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","shutdownNotification","reset"],"hd":"include/mixr/simulation/AbstractPlayer.hpp","src":"src/simulation/AbstractPlayer.cpp","ml":{"copyData":39,"deleteData":63,"shutdownNotification":78,"reset":92}},"AbstractNetIO":{"b":"Component","f":null,"m":"simulation","r":false,"ch":["AbstractNetIO","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/simulation/AbstractNetIO.hpp","src":"src/simulation/AbstractNetIO.cpp","ml":{}},"AbstractDataRecorder":{"b":"AbstractRecorderComponent","f":null,"m":"simulation","r":false,"ch":["AbstractDataRecorder","AbstractRecorderComponent","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/simulation/AbstractDataRecorder.hpp","src":"src/simulation/AbstractDataRecorder.cpp","ml":{"copyData":21,"deleteData":29}},"AbstractNib":{"b":"Component","f":null,"m":"simulation","r":false,"ch":["AbstractNib","Component"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/simulation/AbstractNib.hpp","src":"src/simulation/AbstractNib.cpp","ml":{}},"AbstractRecorderComponent":{"b":"Component","f":null,"m":"simulation","r":false,"ch":["AbstractRecorderComponent","Component"],"sl":["enabledList","disabledList"],"own":2,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/simulation/AbstractRecorderComponent.hpp","src":"src/simulation/AbstractRecorderComponent.cpp","ml":{"copyData":27,"deleteData":35}},"Simulation":{"b":"Component","f":null,"m":"simulation","r":true,"ch":["Simulation","Component"],"sl":["players","simulationTime","day","month","year","firstWeaponId","numTcThreads","numBgThreads"],"own":8,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","reset","shutdownNotification","updateTC","updateData"],"hd":"include/mixr/simulation/Simulation.hpp","src":"src/simulation/Simulation.cpp","ml":{"copyData":81,"deleteData":153,"reset":186,"shutdownNotification":394,"updateTC":457,"updateData":621}},"Station":{"b":"Component","f":null,"m":"simulation","r":true,"ch":["Station","Component"],"sl":["simulation","networks","igHosts","ioHandler","ownship","tcRate","tcPriority","tcStackSize","fastForwardRate","netRate","netPriority","netStackSize","bgRate","bgPriority","bgStackSize","startupResetTimer","enableUpdateTimers","dataRecorder"],"own":18,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData","reset","updateTC","updateData","shutdownNotification"],"hd":"include/mixr/simulation/Station.hpp","src":"src/simulation/Station.cpp","ml":{"copyData":89,"deleteData":179,"reset":199,"updateTC":258,"updateData":323,"shutdownNotification":358}},"Sz1":{"b":"FirstOrderTf","f":null,"m":"linearsystem","r":false,"ch":["Sz1","FirstOrderTf","DiffEquation","ScalerFunc"],"sl":["n1","N1","n2","N2","d1","D1","d2","D2"],"own":8,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/linearsystem/Sz1.hpp","src":"src/linearsystem/Sz1.cpp","ml":{}},"LimitFunc":{"b":"ScalerFunc","f":null,"m":"linearsystem","r":false,"ch":["LimitFunc","ScalerFunc"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/linearsystem/LimitFunc.hpp","src":"src/linearsystem/LimitFunc.cpp","ml":{"copyData":37}},"SaH":{"b":"ScalerFunc","f":null,"m":"linearsystem","r":false,"ch":["SaH","ScalerFunc"],"sl":["sampleRate"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/linearsystem/SaH.hpp","src":"src/linearsystem/SaH.cpp","ml":{"copyData":44}},"Sz2":{"b":"SecondOrderTf","f":null,"m":"linearsystem","r":false,"ch":["Sz2","SecondOrderTf","DiffEquation","ScalerFunc"],"sl":["n1","N1","n2","N2","n3","N3","d1","D1","d2","D2","d3","D3"],"own":12,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/linearsystem/Sz2.hpp","src":"src/linearsystem/Sz2.cpp","ml":{}},"Limit11":{"b":"LimitFunc","f":null,"m":"linearsystem","r":false,"ch":["Limit11","LimitFunc","ScalerFunc"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/linearsystem/Limit11.hpp","src":"src/linearsystem/Limit11.cpp","ml":{}},"Limit":{"b":"LimitFunc","f":null,"m":"linearsystem","r":false,"ch":["Limit","LimitFunc","ScalerFunc"],"sl":["lower","upper"],"own":2,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/linearsystem/Limit.hpp","src":"src/linearsystem/Limit.cpp","ml":{}},"SecondOrderTf":{"b":"DiffEquation","f":null,"m":"linearsystem","r":false,"ch":["SecondOrderTf","DiffEquation","ScalerFunc"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/linearsystem/SecondOrderTf.hpp","src":"src/linearsystem/SecondOrderTf.cpp","ml":{"copyData":44,"deleteData":62}},"FirstOrderTf":{"b":"DiffEquation","f":null,"m":"linearsystem","r":false,"ch":["FirstOrderTf","DiffEquation","ScalerFunc"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/linearsystem/FirstOrderTf.hpp","src":"src/linearsystem/FirstOrderTf.cpp","ml":{"copyData":45}},"ScalerFunc":{"b":"Object","f":null,"m":"linearsystem","r":false,"ch":["ScalerFunc"],"sl":["rate","x0","y0"],"own":3,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":"include/mixr/linearsystem/ScalerFunc.hpp","src":"src/linearsystem/ScalerFunc.cpp","ml":{"copyData":34,"deleteData":56}},"LagFilter":{"b":"FirstOrderTf","f":null,"m":"linearsystem","r":false,"ch":["LagFilter","FirstOrderTf","DiffEquation","ScalerFunc"],"sl":["tau"],"own":1,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/linearsystem/LagFilter.hpp","src":"src/linearsystem/LagFilter.cpp","ml":{"copyData":35}},"DiffEquation":{"b":"ScalerFunc","f":null,"m":"linearsystem","r":false,"ch":["DiffEquation","ScalerFunc"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData"],"hd":"include/mixr/linearsystem/DiffEquation.hpp","src":"src/linearsystem/DiffEquation.cpp","ml":{"copyData":22}},"LowpassFilter":{"b":"FirstOrderTf","f":null,"m":"linearsystem","r":false,"ch":["LowpassFilter","FirstOrderTf","DiffEquation","ScalerFunc"],"sl":["wc"],"own":1,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/linearsystem/LowpassFilter.hpp","src":"src/linearsystem/LowpassFilter.cpp","ml":{}},"Limit01":{"b":"LimitFunc","f":null,"m":"linearsystem","r":false,"ch":["Limit01","LimitFunc","ScalerFunc"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":"include/mixr/linearsystem/Limit01.hpp","src":"src/linearsystem/Limit01.cpp","ml":{}},"NtmOutputNodeStd":{"b":null,"f":null,"m":null,"r":false,"ch":["NtmOutputNodeStd"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":["copyData","deleteData"],"hd":null,"src":"src/interop/common/NetIO.cpp","ml":{"copyData":1367,"deleteData":1414}},"InfantryMan":{"b":null,"f":null,"m":null,"r":false,"ch":["InfantryMan"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":null,"src":"src/models/player/LifeForm.cpp","ml":{}},"Parachutist":{"b":null,"f":null,"m":null,"r":false,"ch":["Parachutist"],"sl":[],"own":0,"wp":[],"po":{},"d":false,"ov":[],"hd":null,"src":"src/models/player/LifeForm.cpp","ml":{}}};
const FACTORIES = {"base":{"file":"src/base/factory.cpp","classes":["Add","Agent","AngularVelocity","Arbiter","BTUs","Boolean","Calories","CentiMeters","Cie","Cmy","Color","Complex","Days","Decibel","DecibelMilliWatts","DecibelSquareMeters","DecibelWatts","Degrees","Divide","DownTimer","EarthModel","Feet","FileReader","Float","FootPounds","Func1","Func2","Func3","Func4","Func5","GigaHertz","Grams","Hertz","Hls","Horsepower","Hours","Hsv","Hsva","Inches","Integer","Joules","KiloGrams","KiloHertz","KiloMeters","KiloNewtons","KiloWattHours","KiloWatts","LatLon","LinearVelocity","MegaHertz","Meters","MicroMeters","MicroSeconds","Microns","MilliSeconds","MilliWatts","Minutes","Multiply","NanoSeconds","NauticalMiles","Newtons","Number","Polynomial","PoundForces","Poundals","Radians","Rgb","Rgba","Rotation","Scale","Seconds","Semicircles","Slugs","SquareCentiMeters","SquareFeet","SquareInches","SquareKiloMeters","SquareMeters","SquareMiles","SquareMilliMeters","SquareYards","Statistic","StatuteMiles","Subtract","Table1","Table2","Table3","Table4","Table5","TcpClient","TcpServerMultiple","TcpServerSingle","TeraHertz","Translation","UdpBroadcastHandler","UdpMulticastHandler","UdpUnicastHandler","UpTimer","Watts","Yiq"]},"interop/dis":{"file":"src/interop/dis/factory.cpp","classes":["EmissionPduHandler","NetIO","Ntm"]},"interop/rprfom":{"file":"src/interop/rprfom/factory.cpp","classes":["NetIO"]},"linkage":{"file":"src/linkage/factory.cpp","classes":["Ai2DiSwitch","AnalogInput","AnalogInputFixed","AnalogOutput","AnalogSignalGen","DiscreteInput","DiscreteInputFixed","DiscreteOutput","IoData","MockDevice","UsbJoystick"]},"models":{"file":"src/models/factory.cpp","classes":["Aam","ActionCamouflageType","ActionDecoyRelease","ActionImagingSar","ActionWeaponRelease","Agm","AirAngleOnlyTrkMgr","AirTrkMgr","AirVehicle","Aircraft","AircraftIrSignature","Antenna","ArmoredVehicle","Artillery","Autopilot","AvionicsPod","Bomb","BoosterSpaceVehicle","Building","Bullet","Bullseye","Chaff","CollisionDetect","CommRadio","Datalink","Decoy","Flare","FuelTank","Gimbal","Gmti","GmtiTrkMgr","Gps","GroundStation","GroundStationRadar","GroundStationUav","GroundVehicle","Gun","Helicopter","Iff","Ins","IrAtmosphere","IrAtmosphere1","IrBox","IrSeeker","IrSensor","IrShape","IrSignature","IrSphere","JSBSimModel","Jammer","LaeroModel","LifeForm","MannedSpaceVehicle","MergingIrSensor","Missile","MultiActorAgent","Navigation","OnboardComputer","Pilot","Player","RacModel","Radar","Radio","RfSensor","Route","Rwr","RwrTrkMgr","Sam","SamVehicle","Sar","ScanGimbal","SensorMgr","Ship","SigAzEl","SigConstant","SigDihedralCR","SigPlate","SigSphere","SigSwitch","SigTrihedralCR","SimAgent","SimpleStoresMgr","SpaceVehicle","StabilizingGimbal","Steerpoint","Stores","Stt","System","Tank","TargetData","Track","Tws","UnmannedAirVehicle","UnmannedSpaceVehicle","WheeledVehicle","WorldModel"]},"recorder":{"file":"src/recorder/factory.cpp","classes":["DataRecorder","FileReader","FileWriter","NetInput","NetOutput","OutputHandler","PrintPlayer","PrintSelected","TabPrinter"]},"simulation":{"file":"src/simulation/factory.cpp","classes":["Simulation","Station"]},"terrain":{"file":"src/terrain/factory.cpp","classes":["DedFile","DtedFile","QuadMap","SrtmHgtFile"]}};
const SNIPPETS = {
"AgentTC::updateTC":{"file":"src/base/ubf/Agent.cpp","line":170,"lines":["void AgentTC::updateTC(const double dt)","{","   controller(dt);","}"],"trunc":false},
"Agent::controller":{"file":"src/base/ubf/Agent.cpp","line":64,"lines":["void Agent::controller(const double dt)","{","   base::Component* actor{getActor()};","","   if ( (actor!=nullptr) && (getState()!=nullptr) && (getBehavior()!=nullptr) ) {","","      // update ubf state","      getState()->updateState(actor);","","      // generate an action, but allow possibility of no action returned","      AbstractAction* action{getBehavior()->genAction(state, dt)};","      if (action) {","         action->execute(actor);","         action->unref();","      }","   }","}"],"trunc":false},
"AbstractState::updateState":{"file":"src/base/ubf/AbstractState.cpp","line":46,"lines":["void AbstractState::updateState(const base::Component* const actor)","{","   // Update all my children","   base::PairStream* subcomponents{getComponents()};","   if (subcomponents != nullptr) {","      if (isComponentSelected()) {","         // When we've selected only one","         if (getSelectedComponent() != nullptr) {","            const auto state = dynamic_cast<AbstractState*>(getSelectedComponent());","            if (state != nullptr)","               state->updateState(actor);","         }","      } else {","         // When we should update them all","         base::List::Item* item{subcomponents->getFirstItem()};","         while (item != nullptr) {","            const auto pair = static_cast<base::Pair*>(item->getValue());","            const auto obj = static_cast<base::Component*>(pair->object());","            const auto state = dynamic_cast<AbstractState*>(obj);","            if (state != nullptr)","               state->updateState(actor);","            item = item->getNext();","         }","      }","      subcomponents->unref();","      subcomponents = nullptr;","   }","}"],"trunc":false},
"Arbiter::genAction":{"file":"src/base/ubf/Arbiter.cpp","line":38,"lines":["AbstractAction* Arbiter::genAction(const AbstractState* const state, const double dt)","{","   // create list for action set","   const auto actionSet = new base::List();","","   // fill out list of recommended actions by behaviors","   base::List::Item* item{behaviors->getFirstItem()};","   while (item != nullptr) {","      // get a behavior","      const auto behavior = static_cast<AbstractBehavior*>(item->getValue());","      // generate action, we have reference","      AbstractAction* action{behavior->genAction(state, dt)};","      if (action != nullptr) {","         // add to action set","         actionSet->addTail(action);","         // unref our action reference","         action->unref();","      }","      // goto behavior","      item = item->getNext();","   }","","   // given the set of recommended actions, the arbiter","   // decides what action to take","   AbstractAction* complexAction{genComplexAction(actionSet)};","","   // done with action set","   actionSet->unref();","","   // return action to perform","   return complexAction;","}"],"trunc":false},
"Arbiter::genComplexAction":{"file":"src/base/ubf/Arbiter.cpp","line":75,"lines":["AbstractAction* Arbiter::genComplexAction(base::List* const actionSet)","{","   AbstractAction* complexAction{};","   int maxVote{};","","   // process entire action set","   base::List::Item* item{actionSet->getFirstItem()};","   while (item != nullptr) {","","      // Is this action's vote higher than the previous?","      const auto action = static_cast<AbstractAction*>(item->getValue());","      if (maxVote==0 || action->getVote() > maxVote) {","","         // Yes ...","         if (complexAction != nullptr) complexAction->unref();","         complexAction = action;","         complexAction->ref();","         maxVote = action->getVote();","      }","","      // next action","      item = item->getNext();","   }","","   if (maxVote > 0 && isMessageEnabled(MSG_DEBUG))","      std::cout << \"Arbiter: chose action with vote= \" << maxVote << std::endl;","","   // Use our vote value; if its been set","   if (getVote() > 0 && complexAction != nullptr) {","      complexAction->setVote(getVote());","   }","","   // complexAction will have the vote value of whichever component action was selected","   return complexAction;","}"],"trunc":false},
"Station::updateTC":{"file":"src/simulation/Station.cpp","line":258,"lines":["void Station::updateTC(const double dt)","{","   // Update the base::Timers","   if (isUpdateTimersEnabled()) {","      base::Timer::updateTimers(dt);","   }","","   // the I/O handler","   if (ioHandler != nullptr) {","      ioHandler->tcFrame(dt);","   }","","   // Process station inputs","   inputDevices(dt);","","   // Update the simulation","   if (sim != nullptr) sim->tcFrame(dt);","","   // Process station outputs","   outputDevices(dt);","","   // Our major subsystems","   if (sim != nullptr && igHosts != nullptr) {","      base::PairStream* playerList{sim->getPlayers()};","      base::List::Item* item{igHosts->getFirstItem()};","      while (item != nullptr) {","","         const auto pair = static_cast<base::Pair*>(item->getValue());","         const auto p = static_cast<AbstractIgHost*>(pair->object());","","         // Set ownship & player list","         p->setOwnship(ownship);","         p->setPlayerList(playerList);","","         // TC frame","         p->tcFrame(dt);","","         item = item->getNext();","      }","      if (playerList != nullptr) playerList->unref();","   }","","   // Startup RESET timer --","   //    Sends an initial RESET pulse after timeout","   //    (Some simulation may need this)","   if (startupResetTimer >= 0) {","      startupResetTimer -= dt;","      if (startupResetTimer < 0) {","         this->event(RESET_EVENT);","      }","   }","","   // Update the base class data","   BaseClass::updateTC(dt);","}"],"trunc":false},"Station::updateData":{"file":"src/simulation/Station.cpp","line":323,"lines":["void Station::updateData(const double dt)","{","   // Create a background thread (if needed)","   if (getBackgroundRate() > 0 && !doWeHaveTheBgThread()) {","      createBackgroundProcess();","   }","","   // Our simulation model and image generator host interfaces (if no separate thread)","   if (getBackgroundRate() == 0 && !doWeHaveTheBgThread()) {","      processBackgroundTasks(dt);","   }","","   // Create a network thread (if needed)","   if (getNetworkRate() > 0 && networks != nullptr && !doWeHaveTheNetThread()) {","      createNetworkProcess();","   }","","   // Our interoperability networks (if no separate thread)","   if (getNetworkRate() == 0 && networks != nullptr && !doWeHaveTheNetThread()) {","      processNetworkInputTasks(dt);","      processNetworkOutputTasks(dt);","   }","","   // ---","   // Background processing of the data recorders","   // ---","   if (dataRecorder != nullptr) dataRecorder->processRecords();","","   // Update base class data","   BaseClass::updateData(dt);","}"],"trunc":false},"Component::updateTC":{"file":"src/base/Component.cpp","line":243,"lines":["void Component::updateTC(const double dt)","{","    // Update all my children","    PairStream* subcomponents {getComponents()};","    if (subcomponents != nullptr) {","        if (selection != nullptr) {","            // When we've selected only one","            if (selected != nullptr) selected->tcFrame(dt);","        } else {","            // When we should update them all","            List::Item* item{subcomponents->getFirstItem()};","            while (item != nullptr) {","                const auto pair = static_cast<Pair*>(item->getValue());","                const auto obj = static_cast<Component*>( pair->object() );","                obj->tcFrame(dt);","                item = item->getNext();","            }","        }","        subcomponents->unref();","        subcomponents = nullptr;","    }","}"],"trunc":false},"Component::tcFrame":{"file":"src/base/Component.cpp","line":184,"lines":["void Component::tcFrame(const double dt)","{","   // ---","   // Collect start time","   // ---","   double tcStartTime {};","   if (isTimingStatsEnabled()) {","      #if defined(WIN32)","         LARGE_INTEGER fcnt;","         QueryPerformanceCounter(&fcnt);","         tcStartTime = static_cast<double>( fcnt.QuadPart );","      #else","         tcStartTime = getComputerTime();","      #endif","   }","","   // ---","   // Execute one time-critical frame","   // ---","   this->updateTC(dt);","","   // ---","   // Process timing data","   // ---","   if (isTimingStatsEnabled()) {","","      double dtime {};    // Delta time in MS","      #if defined(WIN32)","         LARGE_INTEGER cFreq;","         QueryPerformanceFrequency(&cFreq);","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Component::processComponents":{"file":"src/base/Component.cpp","line":583,"lines":["void Component::processComponents(","      PairStream* const list,","      const std::type_info& filter,","      Pair* const add,","      Component* const remove","   )","{","   PairStream* oldList {components.getRefPtr()};","","   // ---","   // Our dynamic_cast (see below) already filters on the Component class","   // ---","   bool skipFilter {};","   if (filter == typeid(Component)) {","      skipFilter = true;","   }","","   // ---","   // Create a new list, copy (filter) the component pairs and set their container pointers","   // ---","   const auto newList = new PairStream();","   if (list != nullptr) {","","      // Add the (filtered) components to the new list and set their container","      List::Item* item {list->getFirstItem()};","      while (item != nullptr) {","         const auto pair = static_cast<Pair*>(item->getValue());","         const auto cp = dynamic_cast<Component*>( pair->object() );","         if ( cp != nullptr && cp != remove && (skipFilter || cp->isClassType(filter)) ) {","            newList->put(pair);","            cp->container(this);","         } else if ( cp != nullptr && cp == remove ) {","            cp->container(nullptr);","         }","         item = item->getNext();","      }","","   }","","   // ---","   // Add the optional component","   // ---","   if (add != nullptr) {","      const auto cp = dynamic_cast<Component*>( add->object() );","      if ( cp != nullptr && (skipFilter || cp->isClassType(filter)) ) {","         newList->put(add);","         cp->container(this);","      }","   }","","   // ---","   // Swap lists","   // ---","   components = newList;","   newList->unref();","","   // ---","   // Anything selected?","   // ---","   if (selection != nullptr) {","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Player::updateSystemPointers":{"file":"src/models/player/Player.cpp","line":3141,"lines":["void Player::updateSystemPointers()","{","   // ---","   // Set base::Pair pointers for our primary systems located in our list of subcomponents","   // ---","   loadSysPtrs = false;","   setDynamicsModel( findByType(typeid(DynamicsModel)) );","   setDatalink( findByType(typeid(Datalink)) );","   setGimbal( findByType(typeid(Gimbal)) );","   setIrSystem( findByType(typeid(IrSystem)) );","   setNavigation( findByType(typeid(Navigation)) );","   setOnboardComputer( findByType(typeid(OnboardComputer)) );","   setPilot( findByType(typeid(Pilot)) );","   setRadio( findByType(typeid(Radio)) );","   setSensor( findByType(typeid(RfSensor)) );","   setStoresMgr( findByType(typeid(StoresMgr)) );","}"],"trunc":false},"System::updateTC":{"file":"src/models/system/System.cpp","line":84,"lines":["void System::updateTC(const double dt0)","{","   // We're nothing without an ownship ...","   if (ownship == nullptr && getOwnship() == nullptr) return;","","   // ---","   // Delta time","   // ---","","   // real or frozen?","   double dt{dt0};","   if (isFrozen()) dt = 0.0;","","   // Delta time for methods that are running every fourth phase","   double dt4{dt * 4.0};","","   // ---","   // Four phases per frame","   // ---","   WorldModel* sim{ownship->getWorldModel()};","   if (sim == nullptr) return;","","   switch (sim->phase()) {","","      case 0 : // Frame0 --- Dynamics method","         dynamics(dt4);","         break;","","      case 1 : // Frame1 --- Transmit method","         transmit(dt4);","         break;","","      case 2 : // Frame2 --- Receive method","         receive(dt4);","         break;","","      case 3 : // Frame3 --- Process method","         process(dt4);","         break;","   }","","   // ---","   // Last, update our base class","   // and use 'dt' because if we're frozen then so are our subcomponents.","   // ---","   BaseClass::updateTC(dt);","}"],"trunc":false},"Gimbal::dynamics":{"file":"src/models/system/Gimbal.cpp","line":221,"lines":["void Gimbal::dynamics(const double dt)","{","   servoController(dt);","   BaseClass::dynamics(dt);","}"],"trunc":false},"Gimbal::processPlayersOfInterest":{"file":"src/models/system/Gimbal.cpp","line":1318,"lines":["unsigned int Gimbal::processPlayersOfInterest(base::PairStream* const poi)","{","   const auto tdb0 = new Tdb(maxPlayers, this);","","   unsigned int ntgts{tdb0->processPlayers(poi)};","   setCurrentTdb(tdb0);","   tdb0->unref();","","   return ntgts;","}"],"trunc":false},"Antenna::process":{"file":"src/models/system/Antenna.cpp","line":131,"lines":["void Antenna::process(const double dt)","{","   BaseClass::process(dt);","","   // ---","   // Recycle emissions ...","   // Update emission queues: from 'in-use' to 'free'","   // ---","   if (recycle) {","      unsigned int n{inUseEmQueue.entries()};","","      for (unsigned int i = 0; i < n; i++) {","","         base::lock(inUseEmLock);","         Emission* em{inUseEmQueue.get()};","         base::unlock(inUseEmLock);","","         if (em != nullptr && em->getRefCount() > 1) {","            // Others are still referencing the emission, put back on in-use queue","            base::lock(inUseEmLock);","            inUseEmQueue.put(em);","            base::unlock(inUseEmLock);","         } else if (em != nullptr && em->getRefCount() <= 1) {","            // No one else is referencing the emission, push to the free stack","            em->clear();","            base::lock(freeEmLock);","            if (freeEmStack.isNotFull()) freeEmStack.push(em);","            else em->unref();","            base::unlock(freeEmLock);","         }","      }","   }","}"],"trunc":false},"Radar::transmit":{"file":"src/models/system/Radar.cpp","line":153,"lines":["void Radar::transmit(const double dt)","{","   BaseClass::transmit(dt);","","   // Transmitting, scanning and have an antenna?","   if ( !areEmissionsDisabled() && isTransmitting() ) {","      // Send the emission to the other player","      const auto em = new Emission();","      em->setFrequency(getFrequency());","      em->setBandwidth(getBandwidth());","      const double prf1{getPRF()};","      em->setPRF(prf1);","      int pulses{static_cast<int>(prf1 * dt + 0.5)};","      if (pulses == 0) pulses = 1; // at least one","      em->setPulses(pulses);","      const double p{getPeakPower()};","      em->setPower(p);","      em->setMaxRangeNM(getRange());","      em->setPulseWidth(getPulseWidth());","      em->setTransmitLoss(getRfTransmitLoss());","      em->setReturnRequest( isReceiverEnabled() );","      em->setTransmitter(this);","      getAntenna()->rfTransmit(em);","      em->unref();","   }","","}"],"trunc":false},"Autopilot::process":{"file":"src/models/system/Autopilot.cpp","line":187,"lines":["void Autopilot::process(const double dt)","{","   modeManager();","   headingController();","   altitudeController();","   velocityController();","","   BaseClass::process(dt);","}"],"trunc":false},"Navigation::process":{"file":"src/models/navigation/Navigation.cpp","line":191,"lines":["void Navigation::process(const double dt)","{","   BaseClass::process(dt);","","   // ---","   // Update our position, attitude and velocities","   // ---","   if (getOwnship() != nullptr) {","      velValid = updateSysVelocity();","      posValid = updateSysPosition();","      attValid = updateSysAttitude();","      magVarValid = updateMagVar();","   }","   else {","      posValid = false;","      attValid = false;","      velValid = false;","      magVarValid = false;","   }","","   // Update UTC","   double v {utc + dt};","   if (v >= base::time::D2S) {","      v = (v - base::time::D2S);","   }","   setUTC(v);","","   // ---","   // Update our primary route","   // ---","   if (priRoute != nullptr) priRoute->tcFrame(dt);","","   // Update our bullseye","   if (bull != nullptr) bull->compute(this);","","   // ---","   // Update our navigational steering data","   // ---","   updateNavSteering();","}"],"trunc":false},"Navigation::updateData":{"file":"src/models/navigation/Navigation.cpp","line":180,"lines":["void Navigation::updateData(const double dt)","{","   // ---","   // Update the BaseClass and our primary route","   // ---","   if (priRoute != nullptr) priRoute->updateData(dt);","}"],"trunc":false},"TrackManager::process":{"file":"src/models/system/trackmanager/TrackManager.cpp","line":155,"lines":["void TrackManager::process(const double dt)","{","   processTrackList(dt);","   BaseClass::process(dt);","}"],"trunc":false},"SimpleStoresMgr::process":{"file":"src/models/system/SimpleStoresMgr.cpp","line":52,"lines":["void SimpleStoresMgr::process(const double dt)","{","   BaseClass::process(dt);","","   // Weapon released timer","   if (wpnRelTimer > 0.0) {","      // decrease timer to zero","      wpnRelTimer -= dt;","   }","}"],"trunc":false},"Stores::releaseWeapon":{"file":"src/models/system/Stores.cpp","line":313,"lines":["AbstractWeapon* Stores::releaseWeapon(AbstractWeapon* const wpn)","{","   AbstractWeapon* flyout{};","","   Player* own{getOwnship()};","   if (wpn != nullptr && own != nullptr) {","","      // Release the weapon","      wpn->setLaunchVehicle(own);","      flyout = wpn->release();","","   }","","   return flyout;","}"],"trunc":false},"LaeroModel::dynamics":{"file":"src/models/dynamics/LaeroModel.cpp","line":90,"lines":["void LaeroModel::dynamics(const double dt)","{","    update4DofModel(dt);","    dT = dt;","}"],"trunc":false},"RacModel::dynamics":{"file":"src/models/dynamics/RacModel.cpp","line":70,"lines":["void RacModel::dynamics(const double dt)","{","    updateRAC(dt);","}"],"trunc":false},"OnboardComputer::process":{"file":"src/models/system/OnboardComputer.cpp","line":71,"lines":["void OnboardComputer::process(const double dt)","{","   BaseClass::process(dt);","}"],"trunc":false},"ScanGimbal::dynamics":{"file":"src/models/system/ScanGimbal.cpp","line":103,"lines":["void ScanGimbal::dynamics(const double dt)","{","   scanController(dt);","","   // Call BaseClass after to scan controller since the servo controller","   // is located in BaseClass.","   BaseClass::dynamics(dt);","}"],"trunc":false},"Datalink::dynamics":{"file":"src/models/system/Datalink.cpp","line":263,"lines":["void Datalink::dynamics(const double)","{","    //age queues","    mixr::base::Object* tempInQueue[MAX_MESSAGES]{};","    int numIn{};","    Message* msg{};","    while ((numIn < MAX_MESSAGES) && inQueue->isNotEmpty()) {","        mixr::base::Object* tempObj{inQueue->get()};","        msg = dynamic_cast<Message*>(tempObj);","        if (msg != nullptr) {","            if (base::getComputerTime() - msg->getTimeStamp() > msg->getLifeSpan()) {","                //remove message by not adding to list to be put back into queue","                msg->unref();","            } else {","                tempInQueue[numIn++] = msg;","            }","        } else if (tempObj != nullptr) {","            tempInQueue[numIn++] = tempObj;","        }","    }","    if (numIn != 0) {","        for(int i = 0; i < numIn; i++) {","            inQueue->put(tempInQueue[i]);","        }","    }","","    mixr::base::Object* tempOutQueue[MAX_MESSAGES]{};","    int numOut{};","    msg = nullptr;","    while((numOut < MAX_MESSAGES) && outQueue->isNotEmpty()) {","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Radio::receive":{"file":"src/models/system/Radio.cpp","line":212,"lines":["void Radio::receive(const double dt)","{","   BaseClass::receive(dt);","","   // Receiver losses","   const double noise{getRfRecvNoise()};","","   // ---","   // Process Emissions","   // ---","","   Emission* em = nullptr;","   double signal = 0;","","   // Get an emission from the queue","   base::lock(packetLock);","   if (np > 0) {","      np--; // Decrement 'np', now the array index","      em = packets[np];","      signal = signals[np];","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Radar::receive":{"file":"src/models/system/Radar.cpp","line":184,"lines":["void Radar::receive(const double dt)","{","   BaseClass::receive(dt);","","   // Can't do anything without an antenna","   if (getAntenna() == nullptr) return;","","   // Clear the next sweep","   csweep = computeSweepIndex( static_cast<double>(base::angle::R2DCC * getAntenna()->getAzimuth()) );","   clearSweep(csweep);","","   // Compute noise level","   // CGB moved here from RfSystem","   // Basically, we're simulation Hannen's S/I equation from page 356 of his notes.","   // Where I is N + J. J is noise from jamming.","   // Receiver Loss affects the total I, so we have to wait until this point to account for it.","   const double interference{(getRfRecvNoise() + jamSignal) * getRfReceiveLoss()};","   const double noise{getRfRecvNoise() * getRfReceiveLoss()};","   currentJamSignal = jamSignal * getRfReceiveLoss();","   int countNumJammedEm{};","","   // ---","   // Process Returned Emissions","   // ---","","   Emission* em{};","   double signal{};","","   // Get an emission from the queue","   base::lock(packetLock);","   if (np > 0) {","      np--; // Decrement 'np', now the array index","      em = packets[np];","      signal = signals[np];","   }","   base::unlock(packetLock);","","   while (em != nullptr) {","","      // exclude noise jammers (accounted for already in RfSystem::rfReceivedEmission)","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Radar::process":{"file":"src/models/system/Radar.cpp","line":325,"lines":["void Radar::process(const double dt)","{","   BaseClass::process(dt);","","   // Find the track manager","   TrackManager* tm{getTrackManager()};","   if (tm == nullptr) {","      // No track manager! Then just flush the input queue.","      base::lock(myLock);","      for (Emission* em = rptQueue.get(); em != nullptr; em = rptQueue.get()) {","         em->unref();","         rptSnQueue.get();","      }","      base::unlock(myLock);","   }","","   // ---","   // When end of scan, send all unsent reports to the track manager","   // ---","   if (endOfScanFlg) {","","      endOfScanFlg = false;","","      base::lock(myLock);","      for (unsigned int i = 0; i < numReports && i < MAX_REPORTS; i++) {","         if (tm != nullptr) {","            tm->newReport(reports[i], rptMaxSn[i]);","         }","         reports[i]->unref();","         reports[i] = nullptr;","         rptMaxSn[i] = 0;","      }","      numReports = 0;","      base::unlock(myLock);","   }","","","   // ---","   // Process our returned emissions into reports for the track manager","   //   1) Match each emission with existing reports","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Gun::process":{"file":"src/models/system/Guns.cpp","line":136,"lines":["void Gun::process(const double dt)","{","   BaseClass::process(dt);","","   // ---","   // Are we firing?","   // ---","   if (fire && (getRoundsRemaining() > 0 || isUnlimited()) ) {","      const double rps{computeBulletRatePerSecond()};","      const double bpi{rps * dt};","      rcount += bpi;","   }","","   // ---","   // Generate small burst of bullets at 10 hz","   // ---","   burstFrameTimer += dt;","   if (burstFrameTimer >= burstFrameTime) {","      burstFrameTimer = 0;","      if (rcount > 0) burstFrame();","   }","","   // ---","   // Burst timer","   // ---","   if (shortBurstTimer > 0 && fire) {","      shortBurstTimer -= dt;","      if (shortBurstTimer <= 0) {","         shortBurstTimer = 0;","         fire = false;","      }","   }","}"],"trunc":false},"AbstractWeapon::dynamics":{"file":"src/models/player/weapon/AbstractWeapon.cpp","line":253,"lines":["void AbstractWeapon::dynamics(const double dt)","{","   if (isMode(PRE_RELEASE)) {","      // Weapon is on the same side as the launcher","      setSide( getLaunchVehicle()->getSide() );","","      // Launch vehicles rotational matrix","      base::Matrixd lvM{getLaunchVehicle()->getRotMat()};","","      // Set weapon's position at launch","      // 1) Weapon's position is its position relative to the launcher (launcher's body coordinates)","      // 2) Rotate to earth coordinates","      // 3) Add the launcher's position","      const base::Vec2d ip{getInitPosition()};","      const base::Vec3d pos0b(ip.x(), ip.y(), -getInitAltitude());","      const base::Vec3d pos0e{pos0b * lvM}; // body to earth","      const base::Vec3d lpos{getLaunchVehicle()->getPosition()};","      const base::Vec3d pos1{lpos + pos0e};","      setPosition( pos1 );","","      // Weapon's orientation at launch","      const base::Vec3d ia{getInitAngles()};","      base::Matrixd rr;","      base::nav::computeRotationalMatrix( ia[0], ia[1], ia[2], &rr);","      rr *= lvM;","","      setRotMat(rr);","","      // Set velocities are the same as the launcher","      setVelocity( getLaunchVehicle()->getVelocity() );","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Player::dynamics":{"file":"src/models/player/Player.cpp","line":2764,"lines":["void Player::dynamics(const double dt)","{","   // ---","   // Local player ...","   // ---","   if (isLocalPlayer()) {","      // Update the external dynamics model (if any)","      if (getDynamicsModel() != nullptr) {","         // If we have a dynamics model ...","         getDynamicsModel()->freeze( isFrozen() );","         getDynamicsModel()->dynamics(dt);","      }","","      // Update our position","      positionUpdate(dt);","","      if (getNib() != nullptr || true) {","         if (!syncState1Ready) {","            syncState1.setGeocPosition(getGeocPosition());","            syncState1.setGeocVelocity(getGeocVelocity());","            syncState1.setGeocAcceleration(getGeocAcceleration());","            syncState1.setGeocEulerAngles(getGeocEulerAngles());","            syncState1.setAngularVelocities(getAngularVelocities());","            syncState1.setTimeExec(getWorldModel()->getExecTimeSec());","            syncState1.setTimeUtc(getWorldModel()->getSysTimeOfDay());","            syncState1.setValid(true);","            syncState1Ready = true;","            syncState2Ready = false;","            //std::cout << \"Set syncState1\" << std::endl;","         } else {","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Simulation::phaseLoop":{"file":"src/simulation/Simulation.cpp","line":538,"lines":["   // ---","   BaseClass::updateTC(dt0);","","   // ---","   // Called once per frame -- Process 4 phases per frame","   // ---","   {","      // This locks the current player list for this time-critical frame","      base::safe_ptr<base::PairStream> currentPlayerList = players;","","      for (unsigned int f = 0; f < 4; f++) {","","         // Set the current phase","         setPhase(f);","","         if (reqTcThreads == 1) {","            // Our single TC thread","            updateTcPlayerList(currentPlayerList, (dt0/4.0), 1, 1);","         } else if (numTcThreads > 0) {","            // multiple threads","            for (unsigned short i = 0; i < numTcThreads; i++) {","","               // assign the threads from the pool","               unsigned int idx {static_cast<unsigned int>(i+1)};","               tcThreads[i]->start0(currentPlayerList, (dt0/4.0), idx, reqTcThreads);","            }","","            // we're the last thread","            updateTcPlayerList(currentPlayerList, (dt0/4.0), reqTcThreads, reqTcThreads);","","            // Now wait for the other thread(s) to complete","            base::SyncThread** pp {reinterpret_cast<base::SyncThread**>(&tcThreads[0])};","            base::SyncThread::waitForAllCompleted(pp, numTcThreads);","","         } else if (isMessageEnabled(MSG_ERROR)) {","            std::cerr << \"simulation::updateTC() ERROR, invalid T/C thread setup\";","            std::cerr << \"; reqTcThreads = \" << reqTcThreads;","            std::cerr << \"; numTcThreads = \" << numTcThreads;","            std::cerr << std::endl;","         }","      }","   }","","   // Update frame & cycle counts","   int cframe{static_cast<int>(frame() + 1)};","   if (cframe >= 16) {","      incCycle();","      cframe = 0;","   }","   setFrame(cframe);","   setPhase(0);","}","","//------------------------------------------------------------------------------","// Time critical thread processing for every n'th player starting"],"trunc":false},"Simulation::frameCount":{"file":"src/simulation/Simulation.cpp","line":580,"lines":["","   // Update frame & cycle counts","   int cframe{static_cast<int>(frame() + 1)};","   if (cframe >= 16) {","      incCycle();","      cframe = 0;","   }","   setFrame(cframe);","   setPhase(0);","}","","//------------------------------------------------------------------------------","// Time critical thread processing for every n'th player starting","// with the idx'th player","//------------------------------------------------------------------------------","void Simulation::updateTcPlayerList(","   base::PairStream* const playerList,"],"trunc":false},"Player::phaseSwitch":{"file":"src/models/player/Player.cpp","line":546,"lines":["               rfReflect[i]->unref();","               rfReflect[i] = nullptr;","            }","         }","      }","","      // ---","      // Delta time -- real or frozen?","      // ---","      double dt{dt0};","      if (isFrozen()) dt = 0.0;","","      // ---","      // Compute delta time for modules running every fourth phase","      // ---","      double dt4{dt * 4.0};     // Delta time for items running every fourth phase","      switch (getWorldModel()->phase()) {","","         // Phase 0 -- Dynamics","         case 0 : {","            // Our dynamics","            dynamics(dt4);","","            // Log our player's dynamic data just after its been updated ...","            if (dataLogTime > 0.0) {","               // When we have a data logging time, update the timer","               dataLogTimer -= dt4;","               if (dataLogTimer <= 0.0) {","                  // At timeout, log the player's data and ...","","                  BEGIN_RECORD_DATA_SAMPLE( getWorldModel()->getDataRecorder(), REID_PLAYER_DATA )","                     SAMPLE_1_OBJECT( this )","                  END_RECORD_DATA_SAMPLE()","","                  // reset the timer.","                  dataLogTimer = dataLogTime;","               }","            }","","            // Update signatures after we've updated our dynamics","            if (signature != nullptr) signature->updateTC(dt4);","            if (irSignature != nullptr) irSignature->updateTC(dt4);","         }","         break;","","         // Phase 1 -- Sensors transmit","         case 1 :","         break;","","         // Phase 2 -- Sensors Receive","         case 2 :","         break;","","         // Phase 3 -- PDL and other logic","         case 3 :","         break;","","      }","","      // ---","      // Notes:","      //  a) Remember that our subsystems in the components list (e.g., pilot, nav,","      //     sms and obc) are updated by our call to BaseClass:updateTC()","      //  b) We're calling BaseClass::updateTC() class because we want to update","      //     our player dynamics, etc before our subsystems.","      // ---","      BaseClass::updateTC(dt);","   }","}","","//------------------------------------------------------------------------------"],"trunc":false},"Simulation::updateTcPlayerList":{"file":"src/simulation/Simulation.cpp","line":595,"lines":["void Simulation::updateTcPlayerList(","   base::PairStream* const playerList,","   const double dt,","   const unsigned int idx,","   const unsigned int n)","{","   if (playerList != nullptr) {","      unsigned int index{idx};","      unsigned int count{};","      base::List::Item* item {playerList->getFirstItem()};","      while (item != nullptr) {","         count++;","         if (count == index) {","            base::Pair* pair {static_cast<base::Pair*>(item->getValue())};","            AbstractPlayer* ip {static_cast<AbstractPlayer*>(pair->object())};","            ip->tcFrame(dt);","            index += n;","         }","         item = item->getNext();","      }","   }","}",""],"trunc":false}};
const STATS = {"classes":342,"registered":224,"divergent":48,"withSlots":135,"slotsTotal":644,"phaseWork":81,"phaseOwn":37,"dispatch":44,"cpp":313};


const PHASES = [
  { n: 0, m: "dynamics", label: "Dinâmica" },
  { n: 1, m: "transmit", label: "Transmitem" },
  { n: 2, m: "receive", label: "Recebem" },
  { n: 3, m: "process", label: "Lógica" },
];
const DEPTH_LABELS = ["raiz", "executivo e E/S", "players", "sistemas primários", "subsistemas", "detalhe", "ações"];

/* ---------- consultas ao modelo ---------- */
const cls = (c) => MODEL[c] || null;
const chainOf = (c) => (MODEL[c] ? MODEL[c].ch : [c]);
// As 4 raízes do ciclo de decisão (mixr::base::ubf) -- usado só pelo filtro
// "Decisão (UBF)" do Catálogo. Pega Agent/AgentTC/SimAgent/MultiActorAgent
// (via "Agent" na cadeia), Arbiter/qualquer behavior futuro (via
// "AbstractBehavior"), e os dois papéis restantes.
const UBF_ROOTS = ["Agent", "AbstractBehavior", "AbstractState", "AbstractAction"];
const workPhases = (c) => (MODEL[c] ? MODEL[c].wp : []);
const phaseOwner = (c, p) => (MODEL[c] && MODEL[c].po ? MODEL[c].po[String(p)] : null);
const dispatches = (c) => !!(MODEL[c] && MODEL[c].d);
const allSlots = (c) => {
  const out = [];
  chainOf(c).forEach((a) => (MODEL[a] ? MODEL[a].sl : []).forEach((s) => out.push([s, a])));
  return out;
};
const factoryOf = (c) => (MODEL[c] && MODEL[c].f ? MODEL[c].f : c);

/* ============================= cenário ============================== */
/* Só o que é escolha de cenário. Herança, fases e slots vêm do MODEL.  */

const N = (id, c, o = {}) => ({ id, cls: c, children: [], ...o });

// Um único ( Aircraft ) carregando os DEZ sistemas primários que
// Player::updateSystemPointers() resolve por TIPO (Player.cpp:3141-3151) — e,
// dentro de cada um, tudo que a fábrica nativa de mixr::models sabe construir.
// É o mesmo desenho de src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in
// ("qual o player mais elaborado que dá para montar só com componentes NATIVOS
// do mixr::models?"), com uma única diferença deliberada: ali o Datalink é
// ( AlertDatalink ) — a ÚNICA classe não nativa daquele cenário — e aqui é
// ( Datalink ) puro, porque esta página é sobre o framework, não sobre um
// plugin. falcon2 (o alvo, pilha mínima) e o míssil dinâmico completam o
// quadro: side vermelho, alvo do RWR/TWS, e o player que nasce em runtime.
const SCENARIO = N("station", "Station", {
  edl: "station", via: null, thread: "tc",
  children: [
    N("io", "IoHandler", { edl: "io", via: "ioHandler:", thread: "tc",
      note: "Não está registrada em linkage/factory.cpp — nome de fábrica BaseIoHandler. Escrever ( IoHandler ) no EDL não constrói nada." }),
    N("rec", "DataRecorder", { edl: "rec", via: "dataRecorder:", thread: "fundo" }),
    N("net", "NetIO", { edl: "net1", via: "networks:", thread: "rede",
      note: "Nome de fábrica DisNetIO. A mesma classe NetIO existe em dis, hla e rprfom, cada uma com o seu nome." }),
    N("sim", "WorldModel", {
      edl: "sim", via: "simulation:", thread: "tc",
      children: [
        N("terr", "QuadMap", { edl: "terrain", via: "terrain:", thread: "tc" }),

        N("ac", "Aircraft", {
          edl: "falcon1", via: "players:", player: true, thread: "tc",
          note: "53 das 96 classes que mixr::models::factory publica, num Aircraft só — ver 'built-in_mixr_1' no CLAUDE.md.",
          children: [
            // --- 1) DynamicsModel ---------------------------------------
            N("dyn", "JSBSimModel", { edl: "dyn", via: "components:", thread: "tc" }),

            // --- 2) Pilot ------------------------------------------------
            N("ap", "Autopilot", { edl: "ap", via: "components:", thread: "tc",
              note: "leadPlayerName aponta pra ac2 por nome — mesmo mecanismo do antennaName/trackManagerName abaixo, aqui pra formação em vez de sensor." }),

            // --- 3) Navigation --------------------------------------------
            N("nav", "Ins", {
              edl: "nav", via: "components:", thread: "tc+fundo",
              note: "( Ins ) É uma ( Navigation ) (Ins : public Navigation) — por isso o Gps entra como FILHO, não irmão: findByType() pegaria só o primeiro.",
              children: [
                N("gps", "Gps", { edl: "gps", via: "components:", thread: "tc+fundo" }),
                N("bull", "Bullseye", { edl: "bull", via: "bullseye:", thread: "fundo" }),
                N("route", "Route", {
                  edl: "rota", via: "route:", thread: "fundo",
                  note: "autoSequencer() dispara a ( Action ) do steerpoint que a aeronave acabou de passar — por DISTÂNCIA, independe de navMode.",
                  children: [
                    N("wp1", "Steerpoint", { edl: "wp1", via: "components:", thread: "fundo",
                      children: [N("act1", "ActionDecoyRelease", { edl: "wp1.action", via: "action:", thread: "tc" })] }),
                    N("wp2", "Steerpoint", { edl: "wp2", via: "components:", thread: "fundo",
                      children: [N("act2", "ActionImagingSar", { edl: "wp2.action", via: "action:", thread: "tc" })] }),
                    N("wp3", "Steerpoint", { edl: "wp3", via: "components:", thread: "fundo",
                      children: [N("act3", "ActionCamouflageType", { edl: "wp3.action", via: "action:", thread: "tc",
                        note: "Troca camouflageType em runtime — é o índice que SigSwitch::getRCS() usa pra escolher qual dos 6 filhos de assinatura responde." })] }),
                    N("wp4", "Steerpoint", { edl: "wp4", via: "components:", thread: "fundo",
                      children: [N("act4", "ActionWeaponRelease", { edl: "wp4.action", via: "action:", thread: "tc",
                        note: "'station:' não escolhe a estação — trigger() chama sms->releaseOneBomb() e ignora o valor; quem sai é a primeira Bomb livre." })] }),
                  ],
                }),
              ],
            }),

            // --- 4) Datalink ----------------------------------------------
            N("dl", "Datalink", { edl: "dl", via: "components:", thread: "tc",
              note: "Implementa dynamics() — fase 0, não fase 3. Aqui é o Datalink NATIVO — no cenário real este é o único slot ocupado por um plugin (AlertDatalink)." }),

            // --- 5) Radio ---------------------------------------------------
            N("comm", "CommRadio", {
              edl: "comm1", via: "components:", thread: "tc",
              children: [
                N("iff", "Iff", { edl: "iff", via: "components:", thread: "tc",
                  note: "Iff DERIVA de Radio — por isso entra ANINHADO dentro do CommRadio, nunca como irmão (mesma regra do Gps dentro do Ins)." }),
              ],
            }),

            // --- 6) Gimbal ----------------------------------------------
            N("gim", "Gimbal", {
              edl: "antennas", via: "components:", thread: "tc",
              note: "UMA antena por sensor de RF: Antenna::setSystem() guarda um único ponteiro — por isso são 6 antenas, não 1 compartilhada.",
              children: [
                N("a1", "Antenna", { edl: "ant_tws", via: "components:", thread: "tc+fundo" }),
                N("a2", "Antenna", { edl: "ant_stt", via: "components:", thread: "tc+fundo" }),
                N("a3", "Antenna", { edl: "ant_gmti", via: "components:", thread: "tc+fundo" }),
                N("a4", "Antenna", { edl: "ant_rwr", via: "components:", thread: "tc+fundo",
                  note: "Cobertura esférica, ganho baixo — não ilumina nada, só escuta o que os outros transmitem." }),
                N("a5", "Antenna", { edl: "ant_jam", via: "components:", thread: "tc+fundo" }),
                N("stab", "StabilizingGimbal", {
                  edl: "estab", via: "components:", thread: "tc",
                  note: "Gimbal DENTRO de gimbal: a antena do SAR pendurada aqui, contra-rolada. findByName() é recursivo — 'ant_sar' continua alcançável por nome simples.",
                  children: [N("a6", "Antenna", { edl: "ant_sar", via: "components:", thread: "tc+fundo" })],
                }),
                N("irst", "IrSeeker", { edl: "irst", via: "components:", thread: "tc+fundo",
                  note: "É um ScanGimbal (logo um Gimbal) — por isso mora aqui, não solto no player: solto disputaria o ponteiro primário de Gimbal." }),
              ],
            }),

            // --- 7) RfSensor -------------------------------------------
            N("sens", "SensorMgr", {
              edl: "sensors", via: "components:", thread: "tc",
              note: "É um RfSensor: o contêiner que permite mais de um sensor de RF no mesmo player.",
              children: [
                N("tws", "Tws", { edl: "tws", via: "components:", thread: "tc+fundo" }),
                N("stt", "Stt", { edl: "stt", via: "components:", thread: "tc+fundo" }),
                N("gmti", "Gmti", { edl: "gmti", via: "components:", thread: "tc+fundo" }),
                N("sar", "Sar", { edl: "sar", via: "components:", thread: "tc+fundo" }),
                N("rwr", "Rwr", { edl: "rwr", via: "components:", thread: "tc+fundo",
                  note: "disableEmissions:true — só recebe. Não entrega a posição da própria aeronave a quem também tem RWR." }),
                N("jam", "Jammer", { edl: "jam", via: "components:", thread: "tc+fundo" }),
              ],
            }),

            // --- 8) IrSystem ---------------------------------------------
            N("irs", "IrSensor", { edl: "irsystem", via: "components:", thread: "tc+fundo",
              note: "Não é ( MergingIrSensor ): essa exige um AirAngleOnlyTrkMgrPT, referenciado por reset() mas sem branch em models/factory.cpp — não construível neste fork." }),

            // --- 9) OnboardComputer --------------------------------------
            N("obc", "OnboardComputer", {
              edl: "obc", via: "components:", thread: "tc",
              note: "O contêiner de TrackManager. A ordem só importa pra quem pede o 'primário' por tipo — o resto pede por NOME (twsTrkMgr).",
              children: [
                N("ttm", "AirTrkMgr", { edl: "twsTrkMgr", via: "components:", thread: "tc" }),
                N("rtm", "RwrTrkMgr", { edl: "rwrTrkMgr", via: "components:", thread: "tc" }),
                N("gtm", "GmtiTrkMgr", { edl: "gmtiTrkMgr", via: "components:", thread: "tc" }),
                N("itm", "AirAngleOnlyTrkMgr", { edl: "irTrkMgr", via: "components:", thread: "tc" }),
              ],
            }),

            // --- 10) StoresMgr --------------------------------------------
            N("sto", "SimpleStoresMgr", {
              edl: "stores", via: "components:", thread: "tc",
              note: "( StoresMgr ) no EDL constrói ESTA classe — a abstrata StoresMgr registra-se como BaseStoresMgr e não é construível.",
              children: [
                N("s1", "Aam", { edl: "1", via: "stores:", thread: "tc",
                  note: "Fábrica registra como \"AamMissile\" — nome de classe e nome de fábrica divergem." }),
                N("s2", "Aam", { edl: "2", via: "stores:", thread: "tc", dynamic: true }),
                N("s3", "Agm", { edl: "3", via: "stores:", thread: "tc" }),
                N("s4", "Sam", { edl: "4", via: "stores:", thread: "tc" }),
                N("s5", "Bomb", { edl: "5", via: "stores:", thread: "tc",
                  note: "A primeira Bomb livre da lista — é esta que ( ActionWeaponRelease ) do wp4 solta, não importa o 'station:' pedido." }),
                N("s6", "Chaff", { edl: "6", via: "stores:", thread: "tc" }),
                N("s7", "Flare", { edl: "7", via: "stores:", thread: "tc" }),
                N("s8", "Decoy", { edl: "8", via: "stores:", thread: "tc",
                  note: "A primeira Decoy livre — é esta que ( ActionDecoyRelease ) do wp1 solta." }),
                N("s9", "Gun", { edl: "9", via: "stores:", thread: "tc" }),
                N("s10", "FuelTank", { edl: "10", via: "stores:", thread: "—",
                  note: "( ExternalStore ), não arma: a mesma lista de 'stores:' aceita as duas famílias." }),
                N("s11", "AvionicsPod", { edl: "11", via: "stores:", thread: "—" }),
              ],
            }),

            // --- extra: detecção de colisão -----------------------------
            N("col", "CollisionDetect", { edl: "colisao", via: "components:", thread: "tc",
              note: "Não é sistema primário — é um Component comum, atualizado como qualquer outro filho." }),

            // --- assinatura RF comutável ---------------------------------
            N("sig", "SigSwitch", {
              edl: "sig", via: "signature:", thread: "—",
              note: "Não tem slot próprio: getRCS() escolhe o filho de índice camouflageType — é o que ( ActionCamouflageType ) do wp3 troca em runtime.",
              children: [
                N("sg1", "SigSphere", { edl: "sig.limpo", via: "components:", thread: "—" }),
                N("sg2", "SigPlate", { edl: "sig.placa", via: "components:", thread: "—" }),
                N("sg3", "SigConstant", { edl: "sig.const", via: "components:", thread: "—" }),
                N("sg4", "SigDihedralCR", { edl: "sig.died", via: "components:", thread: "—" }),
                N("sg5", "SigTrihedralCR", { edl: "sig.tried", via: "components:", thread: "—" }),
                N("sg6", "SigAzEl", { edl: "sig.azel", via: "components:", thread: "—" }),
              ],
            }),

            // --- assinatura IR (do ALVO, não do sensor) -----------------
            N("irsig", "IrSignature", {
              edl: "irsig", via: "irSignature:", thread: "—",
              note: "( AircraftIrSignature ) seria mais elaborada, mas getAirframeSignature() desreferencia airframeSignatureTable SEM checar nulo — derruba o processo sem as 6 tabelas.",
              children: [N("irsph", "IrSphere", { edl: "irsig.shape", via: "irShapeSignature:", thread: "—" })],
            }),

            // --- ciclo de decisão UBF (mixr::base::ubf) -------------------
            // Sem dado deste repositório: AgentTC/Arbiter/AbstractState/
            // AbstractBehavior são classes REAIS do fork (mesma cadeia/slots
            // do MODEL já usado no resto da página). Os votos 10/6/3 e os 3
            // behaviors são DIDÁTICOS — não correspondem a nenhum
            // comportamento deste repositório (ver a fase 0 da trilha "Thread
            // de Tempo Crítico", onde a decisão de fato roda).
            // Componente do Aircraft, não mais irmão de "sim" ligado por
            // nome: AgentTC não sobrescreve initActor(), então o ator
            // default é o próprio container() -- containment, e por isso
            // este nó tem de morar aqui dentro, como o ÚLTIMO item de
            // components: (mesma posição de FlightAgentTC na produção).
            N("agent", "AgentTC", {
              edl: "agent", via: "components:", thread: "tc",
              note: "AgentTC roda no laço de TEMPO CRÍTICO (updateTC), sem filtro de fase — por isso mora dentro do Aircraft: sem initActor() próprio, o ator é o container() (containment, não actorPlayerName: como o SimAgent).",
              children: [
                N("ubfstate", "AbstractState", {
                  edl: "state", via: "state:", thread: "—",
                  note: "updateState(actor) monta a percepção a cada ciclo. O corpo em AbstractState já é real: só recursa nos filhos (é um estado composto) — quem de fato lê algo do ator é uma subclasse própria." }),
                N("ubfarb", "Arbiter", {
                  edl: "behavior", via: "behavior:", thread: "—",
                  note: "UbfArbiter — ele MESMO é um AbstractBehavior, com uma lista de behaviors filhos (slot behaviors:).",
                  children: [
                    N("ubfbeh1", "AbstractBehavior", { edl: "b1", via: "behaviors:", thread: "—", note: "Exemplo didático — voto 10 (o maior)." }),
                    N("ubfbeh2", "AbstractBehavior", { edl: "b2", via: "behaviors:", thread: "—", note: "Exemplo didático — voto 6." }),
                    N("ubfbeh3", "AbstractBehavior", { edl: "b3", via: "behaviors:", thread: "—", note: "Exemplo didático — voto 3." }),
                  ],
                }),
              ],
            }),
          ],
        }),

        N("ac2", "Aircraft", {
          edl: "falcon2", via: "players:", player: true, thread: "tc",
          note: "Pilha mínima, de propósito: contraste e alvo do TWS/RWR de ac.",
          children: [
            N("dyn2", "RacModel", { edl: "dyn", via: "components:", thread: "tc" }),
            N("sig2", "SigConstant", { edl: "sig", via: "signature:", thread: "—" }),
          ],
        }),

        N("flyout", "Aam", { edl: "addNewPlayer()", via: "addNewPlayer()", player: true, dynamic: true, thread: "tc" }),
      ],
    }),
  ],
});

function normalize(n, parent) {
  n.parent = parent ? parent.id : null;
  n.phases = workPhases(n.cls);
  n.disp = dispatches(n.cls);
  (n.children || []).forEach((c) => normalize(c, n));
  return n;
}
normalize(SCENARIO, null);
const flat = (n, out = []) => (out.push(n), (n.children || []).forEach((c) => flat(c, out)), out);
const ALL = flat(SCENARIO);
const byId = Object.fromEntries(ALL.map((n) => [n.id, n]));
const IN_SCENARIO = new Set(ALL.map((n) => n.cls));
const ancestors = (id) => { const o = []; let c = byId[id]; while (c && c.parent) { o.push([c.parent, c.id]); c = byId[c.parent]; } return o; };

// `dy`: ajuste vertical fino do rotulo, em cima do y natural ((a.y+b.y)/2) --
// ver o comentario perto de onde NAME_LINKS e desenhado. Links que saem do
// MESMO no colapsariam no mesmo ponto sem isto (dois links do mesmo "from"
// caem na mesma zona de folga, e o rotulo "via:"/"dt:" do proprio no ocupa o
// centro da linha) -- valores calibrados olhando o resultado renderizado,
// nao adivinhados.
// `dy` NAO foi adivinhado: a coluna "sensors"/"antennas"/"stores" empilha
// dezenas de nos a cada ROW (42px), cada um com seu proprio rotulo "via:"
// (banda de 12px, folga de so 30px entre uma banda e a proxima) -- o meio-
// termo natural ((a.y+b.y)/2) de varios destes links caia bem em cima da
// banda de ALGUM no da coluna, ou de OUTRO link do mesmo "from". Calculado
// medindo as posicoes renderizadas de verdade (script python offline, nao
// tentativa-e-erro visual) e escolhendo o CENTRO do vao de 30px mais proximo
// do meio-termo natural -- ver doc-polish-progress.md para o metodo.
const NAME_LINKS = [
  { from: "tws", to: "a1", slot: "antennaName:", dy: -7 },
  { from: "tws", to: "ttm", slot: "trackManagerName:", dy: -7 },
  { from: "stt", to: "ttm", slot: "trackManagerName:", dy: 14 },
  { from: "gmti", to: "gtm", slot: "trackManagerName:", dy: 0 },
  { from: "rwr", to: "rtm", slot: "trackManagerName:", dy: 0 },
  { from: "sar", to: "a6", slot: "antennaName:", dy: 0 },
  { from: "irs", to: "itm", slot: "trackManagerName:", dy: -20 },
  { from: "irs", to: "irst", slot: "seekerName:", dy: 10 },
  { from: "ap", to: "ac2", slot: "leadPlayerName:", dy: 0 },
];

/* =============================== EDL ================================ */

// Condensado do MESMO cenário real que o motivou --
// src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in ("qual o player
// mais elaborado que dá para montar só com componentes NATIVOS do
// mixr::models?") -- com uma troca deliberada: datalink: ( Datalink ) puro no
// lugar de ( AlertDatalink ), a única classe NÃO nativa daquele cenário. Aqui
// é só sobre o framework.
const EDL_TEXT = `( Station
   tcRate: 50            // thread TC a 50 Hz
   bgRate: 20            // thread de fundo -- taxa PROPRIA
   netRate: 50
   ownship: "falcon1"
   startupResetTimer: ( Seconds 0.1 )   // sem isto nada roda

   ioHandler:    ( IoHandler )     // NAO registrada na fabrica
   dataRecorder: ( DataRecorder )
   networks:   { net1: ( DisNetIO ) }

   simulation: ( WorldModel
      terrain: ( QuadMap )

      players: {

         // ===========================================================
         // falcon1 -- os DEZ sistemas primarios que
         // Player::updateSystemPointers() acha por TIPO, com tudo que a
         // fabrica nativa sabe construir dentro de cada um
         // ===========================================================
         falcon1: ( Aircraft
            side: blue   type: "A4"   id: 101
            initPosition: [ 0 0 -1750 ]

            // -- assinatura RF comutavel: 6 filhos, camouflageType escolhe
            signature: ( SigSwitch
               components: {
                  limpo: ( SigSphere radius: 3.0 )
                  placa: ( SigPlate a: ( Meters 6.0 ) b: ( Meters 2.0 ) )
                  const: ( SigConstant rcs: ( SquareMeters 2.5 ) )
                  died:  ( SigDihedralCR a: ( Meters 1.5 ) b: ( Meters 1.5 ) )
                  tried: ( SigTrihedralCR a: ( Meters 1.5 ) b: ( Meters 1.5 ) )
                  azel:  ( SigAzEl inDegrees: true inDecibel: true
                           table: ( Table2 x: [ ... ] y: [ ... ] data: { [ ... ] } ) )
               }
            )

            // -- assinatura IR: e do ALVO, nao do sensor
            irSignature: ( IrSignature
               baseHeatSignature: 320.0   emissivity: 0.75
               effectiveArea: ( SquareMeters 3.0 )
               irShapeSignature: ( IrSphere radius: ( Meters 2.0 ) )
            )

            components: {

               // --- 1) DynamicsModel -------------------------------
               dyn: ( JSBSimModel
                  rootDir: "./dist/share/mixr-plugins/flight/jsbsim/"
                  model: "A4"
               )

               // --- 2) Pilot ----------------------------------------
               ap: ( Autopilot
                  navMode: false
                  headingHoldMode: true   altitudeHoldMode: true
                  velocityHoldMode: true
                  leadPlayerName: falcon2       // resolvido por STRING
               )

               // --- 3) Navigation -----------------------------------
               // ( Ins ) E uma ( Navigation ) -- Gps entra como FILHO,
               // nao irmao (findByType() pegaria so o primeiro).
               nav: ( Ins
                  bullseye: ( Bullseye
                     latitude: ( Degrees -22.25 ) longitude: ( Degrees -42.48 )
                  )
                  route: ( Route
                     to: 1   autoSequence: true
                     components: {
                        wp1: ( Steerpoint
                           stptType: DEST   xPos: ( Meters 9290 ) yPos: ( Meters 3000 )
                           action: ( ActionDecoyRelease numToLaunch: 2 interval: ( Seconds 1.0 ) )
                        )
                        wp2: ( Steerpoint
                           stptType: TGT   xPos: ( Meters 6000 ) yPos: ( Meters 7370 )
                           action: ( ActionImagingSar
                              sarLatitude:  ( LatLon direction: "s" degrees: 22 minutes: 12 )
                              sarLongitude: ( LatLon direction: "w" degrees: 42 minutes: 24 )
                           )
                        )
                        wp3: ( Steerpoint
                           stptType: FIX   xPos: ( Meters 3230 ) yPos: ( Meters 4000 )
                           action: ( ActionCamouflageType camouflageType: 1 )
                        )
                        wp4: ( Steerpoint
                           stptType: IP   xPos: ( Meters 6000 ) yPos: ( Meters 1600 )
                           action: ( ActionWeaponRelease
                              targetLatitude:  ( LatLon direction: "s" degrees: 22 minutes: 18 )
                              targetLongitude: ( LatLon direction: "w" degrees: 42 minutes: 33 )
                              station: 5              // IGNORADO por trigger()
                           )
                        )
                     }
                  )
                  components: { gps: ( Gps ) }
               )

               // --- 4) Datalink -------------------------------------
               // nativo aqui -- no cenario real este slot e o UNICO
               // ocupado por um plugin (AlertDatalink)
               dl: ( Datalink )

               // --- 5) Radio ----------------------------------------
               // Iff E um Radio -- entra ANINHADO, nao irmao
               comm1: ( CommRadio
                  radioID: 1   numChannels: 4   channel: 1
                  components: {
                     iff: ( Iff
                        mode1: 3   mode2: 4096   mode3a: 1200
                        enableMode1: true   enableMode2: true   enableMode3a: true
                     )
                  }
               )

               // --- 6) Gimbal ---------------------------------------
               // UMA antena por sensor -- Antenna::setSystem() guarda
               // UM ponteiro, por isso sao 6, nao 1 compartilhada
               antennas: ( Gimbal
                  components: {
                     ant_tws: ( Antenna polarization: horizontal gain: ( dB 42 ) )
                     ant_stt: ( Antenna polarization: horizontal gain: ( dB 44 ) )
                     ant_gmti: ( Antenna polarization: vertical gain: ( dB 38 ) )
                     ant_rwr: ( Antenna polarization: vertical gain: ( dB 3 ) )
                     ant_jam: ( Antenna polarization: vertical gain: ( dB 20 ) )
                     // gimbal DENTRO de gimbal: a antena do SAR contra-rolada
                     estab: ( StabilizingGimbal
                        stabilizingMode: roll
                        components: { ant_sar: ( Antenna polarization: vertical gain: ( dB 40 ) ) }
                     )
                     // e um ScanGimbal (logo um Gimbal) -- por isso mora
                     // aqui, nao solto no player
                     irst: ( IrSeeker searchVolume: [ 0.5236 0.1745 ] numBars: 2 )
                  }
               )

               // --- 7) RfSensor -------------------------------------
               // SensorMgr E um RfSensor: o conteiner que permite mais
               // de um sensor de RF no mesmo player
               sensors: ( SensorMgr
                  components: {
                     tws: ( Tws trackManagerName: twsTrkMgr antennaName: ant_tws
                                frequency: ( GigaHertz 3.0 ) PRF: ( Hertz 500.0 ) )
                     stt: ( Stt trackManagerName: twsTrkMgr antennaName: ant_stt
                                frequency: ( GigaHertz 3.0 ) PRF: ( Hertz 2000.0 ) )
                     gmti: ( Gmti trackManagerName: gmtiTrkMgr antennaName: ant_gmti
                                  frequency: ( GigaHertz 9.5 ) )
                     sar: ( Sar antennaName: ant_sar chipSize: 512 )
                     rwr: ( Rwr trackManagerName: rwrTrkMgr antennaName: ant_rwr
                                disableEmissions: true )         // so RECEBE
                     jam: ( Jammer antennaName: ant_jam disableEmissions: true )
                  }
               )

               // --- 8) IrSystem --------------------------------------
               // NAO ( MergingIrSensor ): exige AirAngleOnlyTrkMgrPT, sem
               // branch em models/factory.cpp -- nao construivel neste fork
               irsystem: ( IrSensor
                  seekerName: irst   trackManagerName: irTrkMgr
                  sensorType: "contrast"
               )

               // --- 9) OnboardComputer -------------------------------
               // o conteiner de TrackManager -- resto do sistema pede
               // por NOME (twsTrkMgr), a ordem so importa pro findByType()
               obc: ( OnboardComputer
                  components: {
                     twsTrkMgr: ( AirTrkMgr maxTracks: 20 alpha: 1.0 beta: 0.5 )
                     rwrTrkMgr: ( RwrTrkMgr maxTracks: 20 alpha: 2.0 )
                     gmtiTrkMgr: ( GmtiTrkMgr maxTracks: 20 alpha: 1.0 beta: 0.5 )
                     irTrkMgr: ( AirAngleOnlyTrkMgr
                        maxTracks: 20   azimuthBin: ( Degrees 5 ) elevationBin: ( Degrees 5 )
                     )
                  }
               )

               // --- 10) StoresMgr -------------------------------------
               // nome de fabrica de SimpleStoresMgr e "StoresMgr" -- a
               // classe abstrata StoresMgr registra-se como BaseStoresMgr
               stores: ( StoresMgr
                  numStations: 11
                  stores: {
                     1: ( AamMissile id: 501 type: "AIM-9"  maxTOF: ( Seconds 60 ) )
                     2: ( AamMissile id: 502 type: "AIM-9"  maxTOF: ( Seconds 60 ) )
                     3: ( AgmMissile id: 503 type: "AGM-65" maxTOF: ( Seconds 90 ) )
                     4: ( Sam        id: 504 type: "SAM-demo" )
                     5: ( Bomb       id: 505 type: "MK-82" arming: free_fall )
                     6: ( Chaff      id: 506 type: "chaff" )
                     7: ( Flare      id: 507 type: "flare" )
                     8: ( Decoy      id: 508 type: "decoy" )
                     9: ( Gun type: "M61A1" rounds: 510 rate: 6000
                              bulletType: ( Bullet id: 509 type: "20mm" ) )
                     10: ( FuelTank  type: "tanque-ventral" jettisonable: true )
                     11: ( AvionicsPod type: "pod-recon" )
                  }
               )

               // --- extra: nao e sistema primario, e Component comum --
               colisao: ( CollisionDetect collisionRange: ( Meters 100 ) maxPlayers: 20 )
            }
         )

         // ===========================================================
         // falcon2 -- pilha MINIMA: contraste e alvo do TWS/RWR de falcon1
         // ===========================================================
         falcon2: ( Aircraft
            side: red   type: "A4"   id: 102
            signature: ( SigConstant rcs: ( SquareMeters 12.0 ) )
            components: { dyn: ( RacModel ) }
         )
      }
   )
)`.split("\n");

const EDL_RANGE = {
  station: [0, 9], io: [7, 7], rec: [8, 8], net: [9, 9],
  sim: [11, 212], terr: [12, 12],
  ac: [21, 201], dyn: [48, 51], ap: [54, 59],
  nav: [64, 97], gps: [96, 96], bull: [65, 67], route: [68, 95],
  wp1: [71, 74], act1: [73, 73], wp2: [75, 81], act2: [77, 80],
  wp3: [82, 85], act3: [84, 84], wp4: [86, 93], act4: [88, 92],
  dl: [102, 102], comm: [106, 114], iff: [109, 112],
  gim: [119, 135], a1: [121, 121], a2: [122, 122], a3: [123, 123],
  a4: [124, 124], a5: [125, 125], stab: [127, 130], a6: [129, 129],
  irst: [133, 133],
  sens: [140, 153], tws: [142, 143], stt: [144, 145], gmti: [146, 147],
  sar: [148, 148], rwr: [149, 150], jam: [151, 151],
  irs: [158, 161],
  obc: [166, 175], ttm: [168, 168], rtm: [169, 169], gtm: [170, 170], itm: [171, 173],
  sto: [180, 196], s1: [183, 183], s2: [184, 184], s3: [185, 185], s4: [186, 186],
  s5: [187, 187], s6: [188, 188], s7: [189, 189], s8: [190, 190], s9: [191, 192],
  s10: [193, 193], s11: [194, 194],
  col: [199, 199],
  sig: [26, 36], sg1: [28, 28], sg2: [29, 29], sg3: [30, 30], sg4: [31, 31],
  sg5: [32, 32], sg6: [33, 34],
  irsig: [39, 43], irsph: [42, 42],
  ac2: [206, 210], dyn2: [209, 209], sig2: [208, 208],
  flyout: [183, 184],
};

// Ilustrativo -- NAO e o EDL de produção deste repositório. A forma dos
// slots é real (state/behavior/behaviors/vote — os mesmos nomes que aparecem
// em MODEL para AgentTC/Agent/Arbiter/AbstractBehavior); o conteúdo concreto
// (classe do state, número de behaviors, votos) é didático. Sem
// actorPlayerName: -- essa é a diferença que justifica "components:" aqui:
// AgentTC não sobrescreve initActor(), então resolve o ator por containment
// (é filho do Aircraft), não por nome. Ver a nota no subtree de SCENARIO e a
// fase 0 da trilha "Thread de Tempo Crítico".
const UBF_EDL_TEXT = `// ilustrativo -- forma real dos slots, conteudo didatico (nao e a configuracao deste repositorio)
// AgentTC roda no laco de TEMPO CRITICO (updateTC), nao no de fundo -- por
// isso e um COMPONENTE do Aircraft (resolve o ator por containment: default
// initActor() usa container()), nao mais por actorPlayerName:.
// "UbfAgentTC" nao esta encadeada em base/factory.cpp (so "UbfAgent" esta) --
// um projeto real precisaria de uma subclasse propria, com fabrica propria,
// pra isto valer em EDL de verdade.
components: {                          // do Aircraft (falcon1) -- ver a aba EDL de "ac"
   ...
   agent: ( UbfAgentTC
      state:    ( AbstractState )         // concreto: uma subclasse propria
      behavior: ( UbfArbiter
         behaviors: {
            ( AbstractBehavior vote: 10 )  // exemplo didatico
            ( AbstractBehavior vote: 6  )
            ( AbstractBehavior vote: 3  )
         }
      )
   )
}`.split("\n");

const UBF_EDL_RANGE = {
  agent: [9, 18], ubfstate: [10, 10],
  ubfarb: [11, 17], ubfbeh1: [13, 13], ubfbeh2: [14, 14], ubfbeh3: [15, 15],
};

/* ============================ geradores ============================= */

const DT = 0.02, FRAMES = 3, LAUNCH_FRAME = 1;
const fmt = (v) => `${(v * 1000).toFixed(1)} ms`;

/* ------------------------------------------------------------------------ *
 * windowLines() -- a razao de nao existir MAIS caixa de rolagem em cima de
 * codigo/EDL: em vez de mostrar o arquivo inteiro numa caixa de altura fixa
 * com overflow:auto (e um scrollTop calculado em JS pra "pular" ate a linha
 * certa), corta-se aqui, ANTES do render, uma janela de no maximo 'max'
 * linhas centrada no trecho destacado. A caixa cresce so ate o que sobrou --
 * nunca mais alto que o conteudo, nunca com barra de rolagem.
 * ------------------------------------------------------------------------ */
function windowLines(lines, hl, max) {
  const n = lines.length;
  if (n <= max) return { lines, offset: 0, cutBefore: false, cutAfter: false };
  const [hs, he] = hl || [0, 0];
  const mid = Math.floor((hs + he) / 2);
  let start = mid - Math.floor(max / 2);
  start = Math.max(0, Math.min(start, n - max));
  return {
    lines: lines.slice(start, start + max),
    offset: start,
    cutBefore: start > 0,
    cutAfter: start + max < n,
  };
}

/* método de fonte a mostrar para um nó numa fase */
function srcFor(node, ph) {
  const owner = phaseOwner(node.cls, ph);
  const key = owner ? `${owner}::${PHASES[ph].m}` : null;
  if (key && SNIPPETS[key]) return key;
  if (node.player) return "Player::phaseSwitch";
  if (node.disp) return "System::updateTC";
  return "Component::updateTC";
}

function traceFrames() {
  const steps = [];
  let stack = [], released = false, exec = 0, simT = 0;
  const push = (s) => steps.push({ ...s, i: steps.length, stack: [...stack], counters: { ...s.counters, exec, simT } });

  for (let fr = 0; fr < FRAMES; fr++) {
    const ctr = { cycle: 0, frame: fr, phase: null };
    stack = [{ label: "Station::updateTC", node: "station" }];
    push({ kind: "frame", node: "station", counters: ctr, dt: DT, src: "Station::updateTC", hl: [2, 10],
      title: `Quadro ${fr} — Station::updateTC(dt)`,
      body: `dt = ${fmt(DT)}. Os Timers avançam antes de tudo, para que isExpired() responda certo durante o resto do quadro; depois o hardware é lido. A ordem dos sete passos é fixa.` });

    stack.push({ label: "Simulation::updateTC", node: "sim" });
    PHASES.forEach((ph) => {
      const c = { ...ctr, phase: ph.n };
      push({ kind: "phase", node: "sim", counters: c, dt: DT / 4, src: "Simulation::phaseLoop", hl: [10, 22],
        title: `setPhase(${ph.n}) — ${ph.label}`,
        body: `A lista de players é percorrida inteira com dt/4 = ${fmt(DT / 4)} antes de a próxima fase começar. Com pool de threads, waitForAllCompleted() é a barreira: ninguém entra na recepção enquanto alguém ainda transmite.`,
        warn: ph.n === 1 ? "O Tdb que esta fase lê foi montado por Gimbal::processPlayersOfInterest() na thread de fundo, possivelmente há um ou dois quadros." : null });

      byId.sim.children.filter((p) => p.player && (!p.dynamic || released)).forEach((p) => {
        stack.push({ label: `${p.cls}::updateTC`, node: p.id });
        walk(p, ph, DT / 4, c, push, stack, fr, () => { released = true; });
        stack.pop();
      });
      exec += 1;
    });

    stack = [{ label: "Station::updateTC", node: "station" }];
    push({ kind: "frameEnd", node: "station", counters: ctr, dt: DT, src: "Simulation::frameCount", hl: [1, 8],
      title: `Fim do quadro ${fr}`,
      body: `frame() vai a ${fr + 1}. A 16 quadros incCycle() dispara e frame() volta a zero. É este contador que o idioma frame() % N == 0 usa para agendar lógica em sub-taxa.` });
    simT += DT;
  }
  return steps;
}

function walk(node, ph, dt, ctr, push, stack, fr, doRelease) {
  // AgentTC::updateTC() é invocado em toda fase pela recursão genérica de
  // Component (que não filtra) -- mas não participa do switch(phase) nem
  // repassa BaseClass::updateTC() aos próprios filhos (confirmado no fonte:
  // nem Agent::updateData nem AgentTC::updateTC chamam BaseClass::
  // update*()): controller() é chamado direto, FORA dessa recursão. Por
  // isso este nó é um beco sem saída pra recursão GENÉRICA (não desce pra
  // state/behavior via o mecanismo de sempre) -- mas a decisão em si RODA
  // aqui, de verdade, no pool de tempo crítico, então esta trilha é o lugar
  // certo de mostrá-la (ver o rename desta trilha p/ "Thread de Tempo
  // Crítico" -- é exatamente por isso). Fase 0: sequência completa (a
  // mesma, byte a byte, que existia numa trilha "Decisão (UBF)" à parte,
  // antes de dobrar aqui). Fases 1-3: controller() roda nelas também --
  // sem filtro de fase -- mas o RESULTADO é idêntico (nada no ciclo lê a
  // fase), então repetir os 7 passos integrais 4x por quadro seria só
  // ruído -- resumido, visível com "Ociosos".
  if (node.id === "agent") {
    if (ph.n !== 0) {
      push({ kind: "visit", node: "agent", counters: ctr, dt, runs: false, idle: true, owner: null,
        src: "AgentTC::updateTC", hl: [0, 3],
        title: `AgentTC::updateTC(dt) — fase ${ph.n}, mesma decisão da fase 0`,
        body: "Sem filtro de fase: controller() roda de novo, mas nada no ciclo lê a fase atual -- é a MESMA decisão da fase 0 deste quadro, repetida." });
      return;
    }
    push({ kind: "decision", node: "agent", counters: ctr, dt, src: "AgentTC::updateTC", hl: [0, 3],
      title: "AgentTC::updateTC(dt) → controller(dt)",
      body: "AgentTC (não Agent) roda no pool de TEMPO CRÍTICO, junto do resto do frame -- a mesma escolha da produção (FlightAgentTC): nenhum relógio de fundo, fora de sincronia, decide por fora.",
      warn: "Sem filtro de fase: controller() é chamado em TODA fase (0..3), 4x por quadro -- as 3 seguintes repetem a MESMA decisão (ligue \"Ociosos\" pra ver). Uma subclasse concreta que só queira decidir uma vez por quadro tem que filtrar ela mesma (ex.: if (phase==3))." });

    stack.push({ label: "Agent::controller", node: "agent" });

    stack.push({ label: "AbstractState::updateState", node: "ubfstate" });
    push({ kind: "decision", node: "ubfstate", counters: ctr, dt, src: "AbstractState::updateState", hl: [0, 27],
      title: "state->updateState(actor) — percepção",
      body: "O corpo em AbstractState já é real: por si só só recursa nos filhos (é um estado composto, como o grafo de components também é). Quem de fato LÊ algo do ator é uma subclasse própria -- no tutorial oficial do MIXR (mainUbf1, ver MIXR-PATTERN-CONTEXT.md §10.1), PlaneState." });
    stack.pop();

    stack.push({ label: "Arbiter::genAction", node: "ubfarb" });
    push({ kind: "decision", node: "ubfarb", counters: ctr, dt, src: "Arbiter::genAction", hl: [0, 31],
      title: "Arbiter::genAction() pergunta a cada behavior",
      body: "UbfArbiter é ele mesmo um AbstractBehavior: percorre a lista behaviors, chama genAction() em cada um e junta as respostas num actionSet -- antes de decidir o que fazer com elas." });

    [["ubfbeh1", "1", 10], ["ubfbeh2", "2", 6], ["ubfbeh3", "3", 3]].forEach(([id, n, vote]) => {
      stack.push({ label: "AbstractBehavior::genAction", node: id });
      push({ kind: "decision", node: id, counters: ctr, dt, src: "Arbiter::genAction", hl: [6, 20],
        title: `behavior ${n} recomenda uma ação — voto ${vote}`,
        body: "Exemplo didático (não é um behavior deste repositório)." });
      stack.pop();
    });
    stack.pop(); // Arbiter::genAction

    stack.push({ label: "Arbiter::genComplexAction", node: "ubfarb" });
    push({ kind: "decision", node: "ubfbeh1", counters: ctr, dt, src: "Arbiter::genComplexAction", hl: [9, 18],
      title: "genComplexAction(): 10 > 6 > 3 — vence o voto 10",
      body: "Critério estrito '>' (Arbiter.cpp): maior voto vence a ação INTEIRA. Empate favorece quem foi listado primeiro na lista behaviors (maxVote==0 também cai nesse ramo).",
      warn: "Isso é o Arbiter PADRÃO. Uma subclasse pode sobrescrever genComplexAction() para compor CAMPO A CAMPO em vez de escolher a ação inteira -- é o que PriorityArbiter faz no tutorial oficial do MIXR (mainUbf1): pitch/roll/heading/throttle, cada um do behavior de maior voto NAQUELE campo. É isso que distingue UBF de uma árvore/máquina de estados onde um único ramo vence de uma vez (MIXR-PATTERN-CONTEXT.md §10.1)." });
    stack.pop();

    push({ kind: "decision", node: "agent", counters: ctr, dt, src: "Agent::controller", hl: [11, 14],
      title: "action->execute(actor); action->unref();",
      body: "A ação é efêmera: nasce em genAction(), atua em execute(actor) e é liberada no mesmo ciclo -- nunca fica guardada. AbstractAction::execute() é puro virtual; qual efeito concreto ela produz é da subclasse (fora de escopo aqui)." });

    stack.pop(); // Agent::controller
    return;
  }

  const runs = node.phases.includes(ph.n);
  const owner = runs ? phaseOwner(node.cls, ph.n) : null;
  const isPlayer = !!node.player;

  push({
    kind: "visit", node: node.id, counters: ctr, dt, runs, idle: !runs, owner,
    src: srcFor(node, ph.n), hl: runs && SNIPPETS[`${owner}::${PHASES[ph.n].m}`] ? [0, 3] : isPlayer ? [16, 22] : node.disp ? [21, 38] : [2, 16],
    title: runs
      ? `${node.cls}::${PHASES[ph.n].m}(dt4)${owner !== node.cls ? ` — herdado de ${owner}` : ""}`
      : `${node.cls}::updateTC(dt) — sem trabalho na fase ${ph.n}`,
    body: runs
      ? `dt recebido = ${fmt(dt)}. ${node.disp ? `System::updateTC() recompõe dt4 = dt*4 = ${fmt(dt * 4)} e despacha` : "O Player recompõe dt4 e despacha"} para ${owner}::${PHASES[ph.n].m}(). A divisão na descida e a multiplicação na chegada se cancelam: o método roda uma vez por quadro, com o dt integral.`
      : node.disp
        ? `dt recebido = ${fmt(dt)}. Nenhuma classe da cadeia ${chainOf(node.cls).slice(0, 3).join(" < ")} implementa ${PHASES[ph.n].m}(); System::${PHASES[ph.n].m}() é um corpo vazio. O nó é visitado e repassa dt aos filhos.`
        : `${node.cls} não deriva de System: não há switch(phase) na cadeia. Component::updateTC() apenas percorre os filhos com tcFrame(dt).`,
    warn: node.note && ph.n === 0 ? node.note : null,
  });

  if (node.id === "ac" && ph.n === 0 && fr === 0)
    push({ kind: "note", node: "ac", counters: ctr, dt, src: "Player::updateSystemPointers", hl: [2, 14],
      title: "loadSysPtrs — a varredura por tipo",
      body: "Os DEZ ponteiros de sistema são resolvidos por findByType() — a mesma razão de só poder haver UM de cada tipo primário: um segundo Navigation irmão seria invisível. É por isso que a ordem dos filhos no EDL é irrelevante." });

  if (node.id === "a1" && ph.n === 1)
    push({ kind: "rf", node: "a1", to: "ac2", counters: ctr, dt, src: "Radar::transmit", hl: [10, 24],
      title: "Radar::transmit() → Antenna::rfTransmit() → alvo->event(RF_EMISSION)",
      body: "Note que quem transmite é o Tws (deriva de Radar), não a antena: Antenna não implementa transmit(). O sensor monta a Emission e chama rfTransmit() da antena nomeada em antennaName. A emissão chega ao alvo como evento.",
      warn: "Aresta invisível a qualquer análise estática: nenhum call graph liga Antenna a Aircraft." });

  if (node.id === "ttm" && ph.n === 3)
    push({ kind: "name", node: "ttm", from: "tws", counters: ctr, dt, src: "TrackManager::process", hl: [0, 4],
      title: "TrackManager::process() — herdado por AirTrkMgr",
      body: "AirTrkMgr não sobrescreve process(). O ponteiro do gerente veio do slot trackManagerName, resolvido por string dentro do Tws — e do Stt: os DOIS sensores apontam para o MESMO twsTrkMgr.",
      warn: "Erro de digitação em trackManagerName não produz erro de carga: o sistema simplesmente não faz nada." });

  if (node.id === "sto" && ph.n === 3 && fr === LAUNCH_FRAME) {
    push({ kind: "release", node: "sto", to: "flyout", counters: ctr, dt, src: "Stores::releaseWeapon", hl: [0, 14],
      title: "Stores::releaseWeapon() — o míssil entra na simulação",
      body: "wpn->release() muda o modo para ACTIVE e chama addNewPlayer(). Do próximo quadro em diante o míssil (a estação 2, um Aam) é percorrido nas quatro fases como qualquer outro player.",
      warn: "A árvore de contenção mudou em execução. Nenhum arquivo de configuração descreve este nó." });
    doRelease();
  }

  (node.children || []).forEach((c) => {
    stack.push({ label: `${c.cls}::updateTC`, node: c.id });
    walk(c, ph, dt, ctr, push, stack, fr, doRelease);
    stack.pop();
  });
}

function traceBackground() {
  const st = [];
  const p = (s) => st.push({ ...s, i: st.length, counters: { cycle: 0, frame: "—", phase: null }, dt: 0.05 });
  p({ kind: "bg", node: "station", src: "Station::updateData", hl: [1, 14], stack: [{ label: "Station::updateData", node: "station" }],
    title: "Station::updateData(dt) — a 20 Hz, não a 50",
    body: "A thread de fundo tem taxa própria (bgRate) e nenhuma relação com as fases. Com bgRate: 0 tudo isto roda sincronamente na thread que chamou updateData().",
    warn: "Não existe dataFrame(): este caminho não passa por invólucro nem é medido." });
  p({ kind: "bg", node: "nav", src: "Navigation::updateData", hl: [0, 6],
    stack: [{ label: "Station::updateData", node: "station" }, { label: "Player::updateData", node: "ac" }, { label: "Navigation::updateData", node: "nav" }],
    title: "Navigation::updateData() — e também Navigation::process()",
    body: "A navegação é dos poucos subsistemas que trabalham nos dois caminhos: updateData() no fundo e process() na fase 3. Os dados de pilotagem que o Autopilot lê podem, portanto, ser de outro quadro." });
  p({ kind: "bg", node: "a1", src: "Gimbal::processPlayersOfInterest", hl: [0, 9],
    stack: [{ label: "Station::updateData", node: "station" }, { label: "RfSystem::updateData", node: "tws" }, { label: "Gimbal::processPlayersOfInterest", node: "a1" }],
    title: "Gimbal::processPlayersOfInterest() monta o Tdb",
    body: "Filtrar centenas de players por alcance, ângulo, tipo e terreno é caro e tolera defasagem, então sai do caminho crítico. Medir a geometria daqueles alvos é barato e precisa ser atual, então fica na fase 1." });
  return st;
}

function traceReset() {
  const st = [];
  const p = (s) => st.push({ ...s, i: st.length, counters: { cycle: 0, frame: "—", phase: null }, dt: 0 });
  p({ kind: "reset", node: "station", src: "Station::updateTC", hl: [42, 51], stack: [{ label: "Station::updateTC", node: "station" }],
    title: "startupResetTimer expira → event(RESET_EVENT)",
    body: "O reset não é chamada de método: é um evento que desce a árvore inteira. Sem startupResetTimer no EDL, a simulação carrega sem erro e não faz nada." });
  p({ kind: "vanish", node: "flyout", src: "Component::processComponents", hl: [0, 20],
    stack: [{ label: "Station::reset", node: "station" }, { label: "Simulation::reset", node: "sim" }],
    title: "players é reconstruída a partir de origPlayers",
    body: "origPlayers vem do slot players: do EDL e nunca é modificada. O míssil lançado nunca esteve lá — desaparece sem que exista uma linha de código para removê-lo.",
    warn: "Entidades vindas da rede e players destruídos seguem a mesma regra." });
  p({ kind: "reset", node: "ac", src: "Component::processComponents", hl: [20, 40],
    stack: [{ label: "Station::reset", node: "station" }, { label: "Simulation::reset", node: "sim" }, { label: "Player::reset", node: "ac" }],
    title: "A porta de tipo de processComponents()",
    body: "Um filho que não seja Component é descartado em silêncio, e nem o parser nem isValid() acusam. O sintoma aparece depois, como um subsistema que não faz nada." });
  return st;
}

// Os quatro momentos "kind" especiais (rf/name/release/vanish) já existem
// dentro de traceFrames()/traceReset() -- misturados aos ~258 passos de
// visita rotineira, fáceis de perder. Esta função não duplica o texto: FILTRA
// as trilhas de verdade (mesma fonte, sempre em sincronia -- editar o texto
// de um evento em walk()/traceReset() atualiza esta lista de graça) e pega só
// a PRIMEIRA ocorrência de cada kind -- "rf"/"name" repetem a cada quadro
// (nenhuma condição de frame nas pushes originais), e um tour didático quer
// UM exemplo de cada, não 8 cópias do mesmo texto.
function eventTour() {
  const frames = traceFrames();
  const reset = traceReset();
  const firstOf = (arr, kind) => arr.find((s) => s.kind === kind);
  return [firstOf(frames, "rf"), firstOf(frames, "name"), firstOf(frames, "release"), firstOf(reset, "vanish")]
    .filter(Boolean)
    .map((s, i) => ({ ...s, i }));
}

// Cor por kind -- as mesmas de segColor() (rf/release/name), mais "vanish"
// (sem uso anterior: reaproveita --muted, a mesma cor neutra de "nunca
// visitado" -- combina com "desaparece", ao contrário do vermelho/roxo/verde
// que marcam uma AÇÃO acontecendo).
const EVENT_KIND_COLOR = { rf: "var(--rf)", release: "var(--new)", name: "var(--ok)", vanish: "var(--muted)" };
const EVENT_KIND_LABEL = {
  rf: "RF: sensor → alvo",
  name: "Nome: slot → componente",
  release: "Liberação: nasce em runtime",
  vanish: "Reset: some sem remover",
};
// Por que cada um NÃO é uma aresta pai-filho comum -- o fio condutor da aba
// Eventos. Curado uma vez aqui, não achado por inferência: a mesma frase
// serviria de legenda pro card de cada evento.
const EVENT_KIND_NOTE = {
  rf: "O sensor emite pro alvo por PARÂMETRO de runtime (o alvo do rfTransmit), não porque são vizinhos na árvore -- nenhum call graph estático liga Antenna a Aircraft.",
  name: "O ponteiro nasce de uma STRING no EDL (trackManagerName), resolvida em runtime -- um erro de digitação não dá erro de carga, só um sistema que nunca faz nada.",
  release: "A árvore de contenção MUDA durante a execução -- addNewPlayer() insere um nó que nenhum arquivo de configuração descreve.",
  vanish: "processComponents() reconstrói a lista de players a partir de origPlayers, que NUNCA teve o míssil -- ele desaparece sem que exista uma linha de código dedicada a removê-lo.",
};

// Devolve a cadeia RAIZ -> id (objetos do SCENARIO, não só ids) -- usada pelo
// "breadcrumb" da aba Eventos pra mostrar de onde cada nó desce, sem
// depender de posição/layout nenhum (ao contrário do grafo principal).
function pathChain(id) {
  const out = [];
  let c = byId[id];
  while (c) { out.unshift(c); c = c.parent ? byId[c.parent] : null; }
  return out;
}

// "Quadro" virou "Thread de Tempo Crítico": a decisão UBF (AgentTC) roda
// nesse MESMO pool -- ela deixou de ser uma trilha à parte ("Decisão
// (UBF)") e passou a aparecer aqui, na fase 0, dentro de walk() (ver o
// bloco "if (node.id === 'agent')" acima). Uma trilha só, sem duplicar a
// mesma sequência em dois lugares.
const TRACES = { tc: { label: "Thread de Tempo Crítico", build: traceFrames }, bg: { label: "Thread de fundo", build: traceBackground }, reset: { label: "Reset", build: traceReset } };

/* ============================== layout ============================== */

// NW subiu de 142 pra 208: o cenário completo tem nomes de classe de até 20
// caracteres (ActionCamouflageType, ActionWeaponRelease) que truncavam em
// elipse mesmo depois do alargamento anterior -- "truncar texto atrapalha"
// (pedido explícito). COL cresceu na mesma proporção (NW + ~70px de goteira,
// o mesmo colchão que já provou caber os rótulos "via:" mais longos).
const NW = 208, NH = 34, ROW = 42, COL = 280;
// Espaçamento equivalente para a árvore VERTICAL (raiz em cima, irmãos lado
// a lado): ROW_V é o passo de profundidade (substitui COL), grande o
// bastante pra caber a caixa (NH) + o cotovelo da aresta + os rótulos
// "via:"/"dt" que antes só cabiam na goteira horizontal. COL_V é o passo
// entre irmãos (substitui ROW), tem de caber a LARGURA da caixa (NW) —
// bem maior que ROW=42, porque agora os irmãos se espalham no eixo que
// antes era só de empilhamento fino.
const ROW_V = 100, COL_V = NW + 46;
// Teto alto de propósito: em zoom baixo os cartões mais lotados (pips de
// visita, badges de thread) ficam ilegíveis -- 10x dá pra ler qualquer
// cartão de perto, inclusive num viewport estreito.
const ZOOM_MIN = 0.4, ZOOM_MAX = 10;

// orientation: "h" (raiz à esquerda, profundidade cresce pra direita -- o
// padrão) ou "v" (raiz em cima, profundidade cresce pra baixo, irmãos lado
// a lado). Mesma árvore, mesmo algoritmo -- só troca qual eixo é
// "profundidade" (a even ROW_V/COL passo por nível) e qual é "espalhamento
// dos irmãos" (o COL_V/ROW passo por folha, na ordem de visita DFS).
function layout(root, orientation) {
  const v = orientation === "v";
  const nodes = []; let i = 0;
  (function place(n, depth) {
    const kids = n.children || [];
    const a = depth * (v ? ROW_V : COL);
    if (!kids.length) {
      const b = i * (v ? COL_V : ROW);
      nodes.push({ ...n, x: v ? b : a, y: v ? a : b, depth });
      i += 1;
    } else {
      kids.forEach((k) => place(k, depth + 1));
      const f = nodes.find((m) => m.id === kids[0].id);
      const l = nodes.find((m) => m.id === kids[kids.length - 1].id);
      const b = v ? (f.x + l.x) / 2 : (f.y + l.y) / 2;
      nodes.push({ ...n, x: v ? b : a, y: v ? a : b, depth });
    }
  })(root, 0);
  return nodes;
}

const THREAD_COLOR = { tc: "var(--ink)", fundo: "var(--bgc)", rede: "var(--ok)", "tc+fundo": "var(--bgc)", "—": "var(--rule)" };

/* =============================== CSS ================================ */

const CSS = `
.mx { --paper:#E6E9E3; --panel:#DCE0D9; --ink:#16232E; --muted:#6E7A76;
  --rule:#C6CDC3; --hot:#B4661E; --rf:#8C2F3D; --bgc:#3D6C8C; --ok:#4A6B4F;
  --new:#7A5B9B; --code:#1B2730; --codeink:#CFD8CE;
  --active-bg:#F0EAE2; --never-bg:#E2E5DF; --running-fg:#F0E2D4;
  --sub-muted:#8F9A93; --band-bg:#E0E4DC; --graph-bg:#EAEDE7;
  --phase-inherited:#8B9691; --phase-now-bg:#F5E7D8; --phase-has-running-bg:#E0C9AF;
  --phase-stroke-running:#D9B48C;
  --seg-phase-0:#9AA79F; --seg-phase-1:#8FA0A8; --seg-phase-2:#A8A08F; --seg-phase-3:#9E93A8;
  --edl-bg:#F0F2EC; --edl-muted:#A3ADA4; --edl-hl:#E2DBCE;
  --code-muted:#5E7280; --code-hl:#2E4250;
  --mono: ui-monospace,'JetBrains Mono','SF Mono',Menlo,monospace;
  --sans: 'Inter',system-ui,-apple-system,sans-serif;
  background:var(--paper); color:var(--ink); font-family:var(--sans);
  font-size:13.5px; line-height:1.5; min-height:100vh;
  /* body/html não têm cor de fundo própria (o template HTML de compile.js não
     define uma) -- sem min-height aqui, um conteúdo mais baixo que a janela
     deixa a sobra transparente, mostrando o branco padrão do navegador por
     baixo. Inofensivo no claro (quase a mesma cor de --paper), mas MUITO
     visível no escuro -- medido: tira de listra branca embaixo da barra de
     transporte (fixed, não conta pra altura do fluxo normal). */
  /* Paleta clara é a padrão (era fixa antes -- agora "Modo escuro", no canto
     superior direito, alterna pra [data-theme="dark"] abaixo; nenhuma das
     duas depende de @media prefers-color-scheme, então o SO do usuário
     nunca decide por conta própria -- só o toggle). */
  color-scheme: light; }
/* Paleta escura: mesmas 26 variáveis, redefinidas -- todo o resto do CSS (e
   os poucos fill=/stroke= que precisam variar por tema, no JS mais abaixo)
   só lê var(--x), nunca um hex cru, então trocar aqui basta. */
.mx[data-theme="dark"] { --paper:#181C19; --panel:#232722; --ink:#E7EAE4; --muted:#8B968E;
  --rule:#3A413B; --hot:#D98A4A; --rf:#E0808F; --bgc:#7FB3D9; --ok:#7FBE8B;
  --new:#B79BDB; --code:#12171B; --codeink:#C7D0C6;
  --active-bg:#2C2F27; --never-bg:#1F231E; --running-fg:#2A1A0A;
  --sub-muted:#77827A; --band-bg:#20251F; --graph-bg:#1D211C;
  --phase-inherited:#5B655D; --phase-now-bg:#3A2A16; --phase-has-running-bg:#4A3620;
  --phase-stroke-running:#6B4E28;
  --seg-phase-0:#4B534C; --seg-phase-1:#445158; --seg-phase-2:#565040; --seg-phase-3:#524A5C;
  --edl-bg:#20251F; --edl-muted:#647169; --edl-hl:#39331F;
  --code-muted:#7C8A93; --code-hl:#293C49;
  color-scheme: dark; }
.mx *:focus-visible { outline:2px solid var(--hot); outline-offset:2px; }
.mx-bar { position:sticky; top:0; z-index:5; background:var(--paper);
  border-bottom:1px solid var(--rule); padding:10px 18px 8px;
  display:flex; justify-content:space-between; align-items:center; gap:14px; flex-wrap:wrap; }
.mx-h1 { font-size:16px; font-weight:600; margin:0; letter-spacing:-0.01em; }
.mx-sub { font-size:12px; color:var(--muted); margin:2px 0 0; }
.mx-tabs { display:flex; gap:3px; flex-wrap:wrap; }
.mx-tab { font:inherit; font-size:12.5px; padding:5px 12px; cursor:pointer;
  border:1px solid var(--rule); background:transparent; color:var(--muted); border-radius:2px; }
.mx-tab:hover { border-color:var(--ink); color:var(--ink); }
.mx-tab[data-on="1"] { background:var(--ink); border-color:var(--ink); color:var(--paper); }
.mx-body { padding:12px 18px 86px; }
.mx-graph { position:relative; border:1px solid var(--rule); border-radius:2px;
  background:var(--graph-bg); margin-bottom:12px; overflow:hidden; }
.mx-svgwrap { height:clamp(300px, 46vh, 780px); width:100%; transition:height 200ms ease-out; }
/* Com o painel de detalhe oculto (botao "detalhe"), o grafo cresce pra usar   *
 * a altura que o painel deixou de ocupar -- e o proposito do toggle. */
.mx-svgwrap[data-expanded="1"] { height:clamp(500px, 82vh, 1500px); }
.mx-svgwrap svg { width:100%; height:100%; display:block; touch-action:none; cursor:grab; }
.mx-svgwrap svg:active { cursor:grabbing; }
.mx-zoom { position:absolute; top:8px; right:8px; display:flex; gap:3px; z-index:2; }
.mx-zbtn { font:inherit; font-size:12px; width:26px; height:26px; padding:0; cursor:pointer;
  border:1px solid var(--rule); background:var(--paper); color:var(--ink); border-radius:2px; }
.mx-zbtn:hover { border-color:var(--ink); }
.mx-zbtn[data-w="1"] { width:auto; padding:0 8px; }
.mx-zoomslider { display:flex; align-items:center; gap:7px; height:26px; padding:0 9px;
  background:var(--paper); border:1px solid var(--rule); border-radius:2px; }
.mx-zoomslider input[type="range"] { width:88px; accent-color:var(--ink); cursor:pointer; }
.mx-zoomslider span { font-size:11px; color:var(--muted); min-width:36px; text-align:right; }

/* --- popup flutuante do clique num no do grafo (fabrica/registro/slots) -- *
 * position:absolute relativo a .mx-graph (que ja e position:relative);     *
 * left/top vem JA CLAMPADOS em JS pra caber dentro de .mx-graph, que corta *
 * overflow -- por isso nenhum tamanho aqui e "auto", os dois lados         *
 * concordam com a mesma largura/altura estimada. z-index acima de         *
 * .mx-zoom (2), acima tambem dos nos do proprio svg. */
.mx-nodepopup { position:absolute; z-index:6; background:var(--paper); border:1px solid var(--ink);
  border-radius:3px; padding:9px 11px 10px; box-shadow:0 3px 10px rgba(0,0,0,0.22);
  animation:mx-fadein 140ms ease-out; }
.mx-nodepopup-x { font:inherit; font-size:14px; line-height:1; padding:0 2px; cursor:pointer;
  border:none; background:none; color:var(--muted); }
.mx-nodepopup-x:hover { color:var(--ink); }
.mx-nodepopup-link { display:block; width:100%; margin-top:8px; font:inherit; font-size:11px;
  padding:5px 6px; cursor:pointer; border:1px solid var(--rule); border-radius:2px;
  background:var(--panel); color:var(--ink); text-align:left; }
.mx-nodepopup-link:hover { border-color:var(--ink); }

.mx-pane { min-width:0; display:flex; flex-direction:column; }
.mx-card { background:var(--panel); padding:11px 13px; border-radius:2px; }
.mx-lbl { font-size:11.5px; color:var(--muted); margin-bottom:5px; display:flex;
  justify-content:space-between; gap:8px; align-items:baseline; }
.mx-mono { font-family:var(--mono); }

/* --- detalhe com abas: cada aba ocupa a largura toda, ninguem disputa espaco --- */
.mx-dtabs { display:flex; gap:3px; flex-wrap:wrap; margin-bottom:10px; }
.mx-dtab { font:inherit; font-size:12.5px; padding:6px 13px; cursor:pointer;
  border:1px solid var(--rule); background:var(--paper); color:var(--muted); border-radius:2px 2px 0 0;
  border-bottom:2px solid transparent; display:flex; align-items:center; gap:6px; }
.mx-dtab:hover { color:var(--ink); }
.mx-dtab[data-on="1"] { color:var(--ink); border-bottom-color:var(--hot); background:var(--panel); font-weight:600; }
.mx-dtab-dot { width:6px; height:6px; border-radius:50%; background:var(--hot); flex-shrink:0; }
.mx-detailbody { animation:mx-fadein 200ms ease-out; }

/* --- codigo/EDL: SEM caixa de rolagem -- a janela de linhas ja vem cortada do   *
 * JS (windowLines()), entao a caixa so cresce ate o que de fato existe.        */
.mx-code { background:var(--code); color:var(--codeink); font-family:var(--mono);
  font-size:11.5px; line-height:18px; padding:10px 0; border-radius:2px; }
.mx-edl { background:var(--edl-bg); border:1px solid var(--rule); font-family:var(--mono);
  font-size:11.5px; line-height:18px; padding:10px 0; border-radius:2px; }
.mx-codecut { text-align:center; font-size:10.5px; color:var(--code-muted); padding:3px 0; letter-spacing:0.06em; }
.mx-edl .mx-codecut { color:var(--edl-muted); }
.mx-cl { display:flex; transition:background 200ms; }
.mx-cl[data-on="1"] { background:var(--code-hl); }
.mx-edl .mx-cl[data-on="1"] { background:var(--edl-hl); }
.mx-num { width:42px; text-align:right; padding-right:9px; color:var(--code-muted); flex-shrink:0; }
.mx-edl .mx-num { width:30px; color:var(--edl-muted); }
.mx-src { white-space:pre; border-left:2px solid transparent; padding-left:8px; }
.mx-cl[data-on="1"] .mx-src { border-left-color:var(--hot); }
.mx-node { cursor:pointer; }
.mx-node rect { transition:fill 130ms, stroke 130ms, opacity 220ms, stroke-width 130ms; }
.mx-node[data-pop="1"] { animation:mx-popin 320ms cubic-bezier(.2,.9,.3,1.3); }

/* --- rotulo do no: foreignObject com ellipsis, NUNCA passa da caixa (era        *
 * <text> puro, que nao quebra nem corta -- em nomes longos o texto vazava por   *
 * cima do proximo elemento; era exatamente o "texto sobreposto" a corrigir). --- */
.mx-fo { pointer-events:none; }
.mx-fo-row { width:100%; height:100%; overflow:hidden; display:flex; align-items:center; }
.mx-fo-cls { font-family:var(--mono); font-weight:600; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
.mx-fo-sub { font-family:var(--mono); white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }

/* --- animacoes de fluxo: aresta "andando" (marcha de formigas) para o caminho   *
 * ativo, halo pulsando no no em execucao, tudo so com CSS/SMIL -- sem lib nova   *
 * (mantem docs/index.html sem nenhuma requisicao de rede pra abrir). --- */
@keyframes mx-dash { to { stroke-dashoffset:-20; } }
@keyframes mx-dashfast { to { stroke-dashoffset:-24; } }
@keyframes mx-halo { 0% { transform:scale(0.55); opacity:0.65; } 100% { transform:scale(2.1); opacity:0; } }
@keyframes mx-popin { from { transform:scale(0.82); opacity:0.3; } to { transform:scale(1); opacity:1; } }
@keyframes mx-fadein { from { opacity:0; transform:translateY(3px); } to { opacity:1; transform:translateY(0); } }
@keyframes mx-glow { 0%,100% { opacity:1; } 50% { opacity:0.45; } }
.mx-edge-onpath { stroke-dasharray:7 5; animation:mx-dash 900ms linear infinite; }
.mx-edge-current { stroke-dasharray:5 4; animation:mx-dashfast 480ms linear infinite; }
.mx-edge-evt { animation:mx-dashfast 420ms linear infinite; }
.mx-halo { fill:none; pointer-events:none; transform-box:fill-box; transform-origin:center;
  animation:mx-halo 1200ms ease-out infinite; }
.mx-phase-now { animation:mx-glow 1000ms ease-in-out infinite; }
.mx-btn { font:inherit; font-size:12.5px; padding:5px 12px; border:1px solid var(--ink);
  background:transparent; color:var(--ink); border-radius:2px; cursor:pointer; }
.mx-btn:hover { background:var(--rule); }
.mx-btn[data-primary="1"] { background:var(--ink); color:var(--paper); }
.mx-transport { position:fixed; left:0; right:0; bottom:0; background:var(--paper);
  border-top:1px solid var(--rule); padding:8px 18px; display:flex; align-items:center;
  gap:8px; flex-wrap:wrap; z-index:6; }
.mx-tl { display:flex; height:24px; gap:1px; flex:1; min-width:170px; cursor:pointer; align-items:flex-end; }
.mx-seg { flex:1; min-width:1px; border-radius:1px 1px 0 0; transition:height 160ms ease-out, opacity 160ms; }
.mx-input { font:inherit; font-size:12.5px; padding:5px 8px; background:var(--paper);
  border:1px solid var(--rule); color:var(--ink); border-radius:2px; }
.mx-chip { display:inline-block; font-family:var(--mono); font-size:11px; padding:1px 6px;
  border-radius:2px; border:1px solid var(--rule); color:var(--muted); }
.mx-cls { display:inline-flex; align-items:center; gap:5px; padding:2px 7px; border-radius:2px;
  font-family:var(--mono); font-size:11.5px; border:1px solid transparent; cursor:pointer; }
.mx-cls:hover { background:var(--panel); }
.mx-cls[data-scn="1"] { border-color:var(--ink); background:var(--panel); }
.mx-cls[data-div="1"] { border-style:dashed; border-color:var(--rf); }
.mx-cls[data-reg="0"] { opacity:0.55; }
.mx-wrap { display:flex; flex-wrap:wrap; gap:4px; }
.mx-mod { border-top:1px solid var(--rule); padding-top:12px; margin-top:16px; }
.mx-warn { margin:8px 0 0; padding-left:8px; border-left:2px solid var(--rf);
  font-size:12px; line-height:1.45; color:var(--rf); }
.mx-leg { display:flex; gap:14px; flex-wrap:wrap; font-size:11.5px; color:var(--muted);
  border-top:1px solid var(--rule); padding:6px 10px; background:var(--paper); align-items:center; }
.mx-leg-toggle { font:inherit; font-size:11.5px; font-weight:600; color:var(--ink); background:var(--panel);
  border:1px solid var(--rule); border-radius:2px; padding:2px 8px; cursor:pointer; }
.mx-leg-toggle:hover { border-color:var(--ink); }
.mx-cardleg { padding:10px 14px 12px; background:var(--paper); border-top:1px solid var(--rule);
  animation:mx-fadein 200ms ease-out; }
.mx-stats { display:flex; gap:16px; flex-wrap:wrap; font-size:12px; color:var(--muted);
  margin-bottom:10px; }
.mx-stats b { color:var(--ink); font-family:var(--mono); font-weight:600; }
.mx-slot { display:flex; gap:8px; font-family:var(--mono); font-size:11px; padding:1px 0; }
.mx-slot span:first-child { min-width:132px; color:var(--ink); }
.mx-slot span:last-child { color:var(--muted); }
/* Grade de colunas em vez de lista alta com scroll: uma cadeia com 40+ slots
 * ainda cabe sem caixa de rolagem, so ficando mais larga que alta. */
.mx-slotgrid { columns:230px; column-gap:18px; }
.mx-slotgrid .mx-slot { break-inside:avoid; }
@media (prefers-reduced-motion:reduce) { .mx * { transition:none !important; animation:none !important; } }
`;

/* =============================== app ================================ */

export default function App() {
  const [mode, setMode] = useState("exec");
  const [focus, setFocus] = useState(null);
  // Espelha `focus` (Catálogo -> Execução), na direção oposta: o popup de nó
  // do grafo (Exec) pede "ver esta classe no Catálogo" e este estado carrega
  // QUAL classe até lá -- consumido (voltando a null) pelo próprio Catalog,
  // mesmo padrão do useEffect de `focus` dentro de Exec.
  const [catalogFocus, setCatalogFocus] = useState(null);
  // Lido uma vez, no mount -- nunca via @media prefers-color-scheme (o CSS
  // acima é explícito sobre isso: só o toggle decide, não o SO). localStorage
  // é só conveniência entre visitas; falha em silêncio (ex.: file:// em
  // navegadores que bloqueiam storage nesse esquema) e cai pro claro.
  const [theme, setTheme] = useState(() => {
    try { return localStorage.getItem("mx-theme") === "dark" ? "dark" : "light"; } catch { return "light"; }
  });
  useEffect(() => { try { localStorage.setItem("mx-theme", theme); } catch { /* sem storage disponível -- ok, só não persiste */ } }, [theme]);
  return (
    <div className="mx" data-theme={theme}>
      <style>{CSS}</style>
      <div className="mx-bar">
        <div>
          <h1 className="mx-h1">MIXR — execução, EDL e {STATS.classes} classes built-in</h1>
          <p className="mx-sub">Extraído da árvore de fontes: {STATS.cpp} arquivos .cpp, {STATS.registered} classes registradas, {STATS.slotsTotal} slots</p>
        </div>
        <div style={{ display: "flex", gap: 8, alignItems: "center" }}>
          <div className="mx-tabs">
            <button className="mx-tab" data-on={mode === "exec" ? 1 : 0} onClick={() => setMode("exec")}>Execução</button>
            <button className="mx-tab" data-on={mode === "events" ? 1 : 0} onClick={() => setMode("events")}>Eventos</button>
            <button className="mx-tab" data-on={mode === "cat" ? 1 : 0} onClick={() => setMode("cat")}>Catálogo</button>
          </div>
          <button className="mx-zbtn" data-w="1" onClick={() => setTheme((t) => (t === "light" ? "dark" : "light"))} title="Alternar modo claro/escuro">
            {theme === "light" ? "☾ escuro" : "☀ claro"}
          </button>
        </div>
      </div>
      {mode === "exec" && (
        <Exec focus={focus} setFocus={setFocus}
              onOpenCatalog={(c) => { setCatalogFocus(c); setMode("cat"); }} />
      )}
      {mode === "events" && <Events />}
      {mode === "cat" && (
        <Catalog onOpen={(c) => { setFocus(c); setMode("exec"); }}
                 openClass={catalogFocus} setOpenClass={setCatalogFocus} />
      )}
    </div>
  );
}

/* ---------------------------- execução ----------------------------- */

function Exec({ focus, setFocus, onOpenCatalog }) {
  const [traceKey, setTraceKey] = useState("tc");
  const [showIdle, setShowIdle] = useState(false);
  const [showNames, setShowNames] = useState(false);
  const [i, setI] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState(650);
  const [pinned, setPinned] = useState(null);
  // Popup flutuante no clique de um nó: nome de fábrica (e se diverge do nome
  // da classe C++), registrada ou não, contagem de slots -- fatos JÁ
  // extraídos em MODEL/FACTORIES mas que a aba Execução nunca mostrava (só o
  // Catálogo, e só depois de buscar a classe à mão). `x`/`y` são relativos ao
  // canto de .mx-graph (graphRef), capturados uma vez no clique -- não
  // recalculados a cada pan/zoom (o popup é uma anotação efêmera, não parte
  // do grafo; ela some ao trocar de nó/trilha/orientação, ver os efeitos
  // abaixo).
  const [popup, setPopup] = useState(null);
  const graphRef = useRef(null);
  const [view, setView] = useState({ k: 1, x: 0, y: 0 });
  const [detailTab, setDetailTab] = useState("step");
  // Oculta o painel de detalhe (Passo/Código/EDL/Classe) pra dar mais altura
  // ao grafo -- pedido explícito, depois que o cenário cresceu pra cobrir
  // todos os modelos built-in possíveis e passou a precisar de mais área de
  // desenho pra caber sem espremer.
  const [showDetail, setShowDetail] = useState(true);
  // Legenda visual do cartão (nome/subtítulo/pips/badge/cor de thread) --
  // fechada por padrão, mesmo raciocínio do "detalhe": explicar sem competir
  // por espaço com o grafo o tempo todo.
  const [showCardLegend, setShowCardLegend] = useState(false);
  // "Seguir ramo": zoom/pan passam a acompanhar sozinhos o caminho raiz->nó
  // ativo (o mesmo caminho que as arestas tracejadas já destacam) a cada
  // passo -- inclusive passo a passo, não só durante "Reproduzir". Ver
  // followViewFor() mais abaixo (onde W/H/topMargin/pos já existem) e a
  // transição condicional no <g> do grafo.
  const [autoFollow, setAutoFollow] = useState(false);
  // "v" gira a árvore pra raiz-em-cima/irmãos-lado-a-lado (ver layout()) --
  // reseta o pan/zoom ao trocar (view.x/y/k de uma orientação não fazem
  // sentido nenhum na outra: os nós inteiros mudam de posição).
  const [orientation, setOrientation] = useState("h");
  const drag = useRef(null);
  // Suprime a transição suave ENQUANTO a barra de zoom está sendo arrastada
  // (mesmo motivo do "!drag.current" para o pan: um <input type="range">
  // dispara onChange a cada tique do arrasto -- animar 420ms a cada tique
  // vira elástico. Some ref, não state: não precisa re-render por si só.
  const sliderActive = useRef(false);
  const transportRef = useRef(null);
  // Altura MEDIDA da barra de transporte (fixed, bottom:0) -- nao um numero fixo.
  // Em viewport estreito ela quebra em 2+ linhas (flex-wrap) e uma folga fixa
  // (o antigo `padding-bottom:86px` do .mx-body) passa a ser MENOR que a barra
  // real, escondendo as ultimas linhas do painel de detalhe atras dela (achado
  // rodando em 650px: 2 linhas do aviso do passo 11/78 sumiam por baixo da
  // barra). ResizeObserver cobre tanto resize de janela quanto qualquer mudanca
  // de conteudo da propria barra (rotulo Reproduzir/Pausar, etc.).
  const [transportH, setTransportH] = useState(0);
  useLayoutEffect(() => {
    const el = transportRef.current;
    if (!el) return undefined;
    const update = () => setTransportH(el.getBoundingClientRect().height);
    update();
    const ro = new ResizeObserver(update);
    ro.observe(el);
    window.addEventListener("resize", update);
    return () => { ro.disconnect(); window.removeEventListener("resize", update); };
  }, []);

  useEffect(() => {
    if (!focus) return;
    const n = ALL.find((x) => x.cls === focus);
    if (n) setPinned(n.id);
    setPopup(null); // chegada por navegação, não por clique -- sem coordenada pra ancorar
    setFocus(null);
  }, [focus, setFocus]);

  const raw = useMemo(() => TRACES[traceKey].build(), [traceKey]);
  const trace = useMemo(() => (showIdle ? raw : raw.filter((s) => !s.idle)), [raw, showIdle]);
  const orientV = orientation === "v";
  const nodes = useMemo(() => layout(SCENARIO, orientation), [orientation]);
  useEffect(() => { setView({ k: 1, x: 0, y: 0 }); setPopup(null); }, [orientation]);
  const pos = useMemo(() => Object.fromEntries(nodes.map((n) => [n.id, n])), [nodes]);

  const idx = Math.min(i, trace.length - 1);
  const step = trace[idx] || {};

  useEffect(() => { setI(0); setPopup(null); }, [traceKey, showIdle]);
  useEffect(() => {
    if (!playing) return;
    const t = setTimeout(() => setI((p) => (p + 1 >= trace.length ? (setPlaying(false), p) : p + 1)), speed);
    return () => clearTimeout(t);
  }, [playing, i, speed, trace.length]);

  const move = useCallback((d) => { setPlaying(false); setI((p) => Math.max(0, Math.min(trace.length - 1, p + d))); }, [trace.length]);
  useEffect(() => {
    const h = (e) => {
      if (e.target.tagName === "INPUT" && e.target.type === "text") return;
      if (e.key === "ArrowRight") move(1);
      else if (e.key === "ArrowLeft") move(-1);
      else if (e.key === " ") { e.preventDefault(); setPlaying((p) => !p); }
    };
    window.addEventListener("keydown", h);
    return () => window.removeEventListener("keydown", h);
  }, [move]);

  const seen = trace.slice(0, idx + 1);
  const launched = traceKey === "tc" && seen.some((s) => s.kind === "release");
  const vanished = traceKey === "reset" && step.kind === "vanish";
  const visits = useMemo(() => {
    const v = {}; seen.forEach((s) => { if (s.kind === "visit") v[s.node] = (v[s.node] || 0) + 1; }); return v;
  }, [idx, traceKey, showIdle]);

  const detail = pinned ? byId[pinned] : byId[step.node] || byId.station;
  const dm = cls(detail.cls) || {};
  const pathEdges = useMemo(() => new Set(ancestors(step.node || "station").map(([a, b]) => a + ">" + b)), [step.node]);

  const snip = SNIPPETS[step.src];
  // Os novos nós de decisão UBF (agent/ubfstate/ubfarb/ubfbeh*) não têm EDL de
  // produção real (ver a nota em UBF_EDL_TEXT) — checa a tabela ilustrativa
  // primeiro, cai para o EDL real dos outros 72 nós senão.
  const ubfEdlRange = UBF_EDL_RANGE[detail.id];
  const edlSrc = ubfEdlRange ? UBF_EDL_TEXT : EDL_TEXT;
  const edlRange = ubfEdlRange || EDL_RANGE[detail.id] || EDL_RANGE.station;
  // Janela de linhas em vez de scroll: ver o "porque" no cabecalho de windowLines().
  const codeWin = useMemo(() => (snip ? windowLines(snip.lines, step.hl, 22) : null), [snip, step.hl]);
  const edlWin = useMemo(() => windowLines(edlSrc, edlRange, 22), [edlSrc, edlRange]);
  // Aba "Codigo"/"EDL" so faz sentido tendo o que mostrar -- se o usuario
  // estava nela e o passo/no atual deixou de ter trecho de fonte (ex.: um no
  // sem override de fase nenhum), cai para "Passo" em vez de mostrar vazio.
  useEffect(() => {
    if (detailTab === "code" && !snip) setDetailTab("step");
  }, [detailTab, snip]);

  const edges = [];
  ALL.forEach((n) => (n.children || []).forEach((c) => edges.push([n.id, c.id])));
  // W/H generalizados pela extensão REAL dos nós, não por maxDepth*COL: na
  // horizontal isso dá exatamente o mesmo valor (x cresce estritamente com a
  // profundidade), mas na vertical quem estica a largura é o número de
  // FOLHAS (x = índice*COL_V), não a profundidade -- uma fórmula só que
  // funciona pras duas sem precisar de um "if" aqui.
  const W = Math.max(...nodes.map((n) => n.x)) + NW + 30;
  const H = Math.max(...nodes.map((n) => n.y)) + NH + 30;
  const ctr = step.counters || {};
  const curPhase = ctr.phase;

  // Horizontal: topo de cada COLUNA (rótulo "raiz"/"players"/... folga do nó
  // mais alto DAQUELA coluna). Vertical: esquerda de cada LINHA (rótulo
  // folga do nó mais à esquerda DAQUELA linha -- mesma ideia, eixo trocado).
  // Nunca um valor fixo: um nó visitado cedo na travessia DFS pode acabar no
  // extremo absoluto (0) -- caso medido de "IoHandler" sob "executivo e E/S"
  // na horizontal, com o rótulo caindo atrás do próprio topo da caixa.
  const depthHeaderPos = useMemo(() => {
    const top = {};
    nodes.forEach((n) => {
      const edge = orientV ? n.x : n.y - NH / 2;
      if (top[n.depth] === undefined || edge < top[n.depth]) top[n.depth] = edge;
    });
    return top;
  }, [nodes, orientV]);
  // Margem de topo (horizontal) ou de esquerda (vertical) do viewBox: tem de
  // acomodar o rótulo mais próximo do extremo -- 12px de folga do nó +
  // ~10px de ascendente da fonte na horizontal; na vertical o rótulo cresce
  // PRA ESQUERDA a partir do nó (texto ancorado à direita, ver o render),
  // então a margem tem de caber a largura do rótulo mais longo
  // ("sistemas primários" ≈ 130px em mono 10px), não só sua altura.
  const topMargin = orientV ? 14 : Math.max(30, -Math.min(...Object.values(depthHeaderPos), 0) + 12 + 10);
  const leftMargin = orientV ? Math.max(150, -Math.min(...Object.values(depthHeaderPos), 0) + 150) : 14;

  // Centraliza o elemento de COMPONENTE DE ATUAÇÃO -- o nó ativo do passo
  // (o mesmo que ganha o halo "executando") -- no zoom ATUAL, sem recalculá-
  // lo. O zoom é escolha do usuário (a barra deslizante); entre passos, ele
  // PERSISTE -- só o enquadramento (pan) acompanha. Antes disto, cada passo
  // recomputava um k próprio (ajustando a caixa do caminho inteiro), e o
  // zoom "pulava" a cada passo -- o oposto de uma barra que o usuário ajusta
  // uma vez e espera que fique.
  const followViewFor = (nodeId, k) => {
    const n = pos[nodeId];
    if (!n) return null;
    // O <g> do grafo tem style={transformOrigin:"center"} -- a ANCORA do
    // transform CSS não é a origem (0,0) do conteúdo, é o CENTRO do viewBox
    // (view-box é o transform-box padrão em SVG). A composição real é
    // origin + k*(p-origin) + (tx,ty); pra centralizar p em `origin`,
    // (tx,ty) = k*(origin-p) -- NÃO (origin-p)*k (o que dava um deslocamento
    // a mais de origin*(k-1), crescendo com o zoom -- medido botando
    // "station" fora da tela com k=1.73 antes deste ajuste).
    // O alvo vertical não é o centro geométrico puro: os botões flutuantes
    // (zoom/detalhe/ajustar, absolutos no canto superior direito) ficam POR
    // CIMA do canvas -- centralizar exatamente no meio deixava o nó, em zoom
    // alto, bater embaixo deles (medido em viewport estreito, com "Station"
    // perto do topo do caminho). Um viés de 6% da altura do viewBox empurra
    // o enquadramento pra baixo, sem custar quase nada do outro lado (o
    // rodapé não tem overlay nenhum).
    const Ox = orientV ? (W - leftMargin) / 2 : (-leftMargin + W / 2);
    const Oy = (H - topMargin) / 2 + (H + topMargin) * 0.06;
    return { k, x: k * (Ox - n.x), y: k * (Oy - n.y) };
  };

  useEffect(() => {
    if (!autoFollow || !step.node) return;
    const v = followViewFor(step.node, view.k);
    if (v) setView(v);
    // view.k entra de propósito: se o usuário reajustar o zoom (a barra)
    // enquanto "Seguir ramo" está ligado, o enquadramento recalcula o pan
    // pro MESMO nó no zoom novo, em vez de deixar o nó fugir do centro.
  }, [autoFollow, idx, step.node, view.k]);

  const bandFor = (rootId) => {
    const ids = flat(byId[rootId]).map((n) => n.id);
    const ys = ids.map((id) => pos[id].y), xs = ids.map((id) => pos[id].x);
    return { y0: Math.min(...ys) - NH / 2 - 5, y1: Math.max(...ys) + NH / 2 + 5, x0: Math.min(...xs) - 6, x1: Math.max(...xs) + NW + 6 };
  };

  const segColor = (s) =>
    s.kind === "rf" ? "var(--rf)" : s.kind === "release" ? "var(--new)" :
    s.kind === "name" ? "var(--ok)" : s.kind === "phase" ? "var(--ink)" :
    s.counters && s.counters.phase != null ? ["var(--seg-phase-0)", "var(--seg-phase-1)", "var(--seg-phase-2)", "var(--seg-phase-3)"][s.counters.phase] : "var(--rule)";

  const onWheel = (e) => { e.preventDefault(); const f = e.deltaY < 0 ? 1.12 : 1 / 1.12; setView((v) => ({ ...v, k: Math.max(ZOOM_MIN, Math.min(ZOOM_MAX, v.k * f)) })); };
  // Captura o pointer só quando o arrasto vira REAL (deslocamento >
  // DRAG_CLICK_PX), não no pointerdown cru. Descoberto rodando um clique de
  // verdade (mousedown+mouseup no MESMO lugar) contra um listener de
  // depuração: capturar cedo demais faz o BROWSER decidir, já no
  // pointerdown, retargetar o "click" resultante pro próprio <svg> capturador
  // -- e essa decisão NÃO muda mesmo soltando a captura depois, no
  // pointerup/onUp (confirmado: hasPointerCapture ia de true a false antes do
  // "click" dispersar, e o "click" ainda saía com target=svg). O onClick de
  // um <g class="mx-node"> por baixo do <svg> nunca disparava com mouse de
  // verdade -- só com dispatchEvent sintético (que não passa por pointer
  // capture nenhum), o que escondeu o bug de um teste anterior. Adiar a
  // captura pro primeiro pointermove que de fato deslocar evita o problema na
  // raiz: um clique sem deslocamento nunca chega a capturar o pointer, então
  // o "click" segue o alvo normal (hit-test no elemento sob o cursor).
  const DRAG_CLICK_PX = 4;
  const onDown = (e) => { drag.current = { x: e.clientX, y: e.clientY, vx: view.x, vy: view.y, captured: false, pointerId: e.pointerId, el: e.currentTarget }; };
  // Captura `d`/cx/cy num LOCAL antes de agendar o setView -- a arvore agora
  // e grande o bastante pra arrastar gerar VARIOS pointermove por frame, e o
  // callback de updater do setState so roda depois (as vezes ja no proximo
  // lote). Ler `drag.current` DENTRO do updater (como era antes) reagia ao
  // valor NA HORA em que o updater executa, nao em que o evento chegou -- um
  // pointerup entre um evento e o outro zera drag.current pra null primeiro,
  // e o updater de um pointermove ainda na fila quebrava com "Cannot read
  // properties of null (reading 'vx')", derrubando a arvore React inteira.
  // Medido travando o app com um arrasto real (nao so no teste automatizado).
  const onMove = (e) => {
    const d = drag.current;
    if (!d) return;
    const cx = e.clientX, cy = e.clientY;
    if (!d.captured) {
      if (Math.hypot(cx - d.x, cy - d.y) < DRAG_CLICK_PX) return; // ainda pode ser so um clique -- nao mexe em nada ainda
      d.captured = true;
      d.el.setPointerCapture(d.pointerId);
    }
    setView((v) => ({ ...v, x: d.vx + (cx - d.x), y: d.vy + (cy - d.y) }));
  };
  const onUp = (e) => {
    if (e && e.currentTarget.hasPointerCapture && e.currentTarget.hasPointerCapture(e.pointerId)) {
      e.currentTarget.releasePointerCapture(e.pointerId);
    }
    drag.current = null;
  };

  const slots = allSlots(detail.cls);

  return (
    <>
      <div className="mx-body" style={{ paddingBottom: Math.max(transportH, 86) + 14 }}>
        <div style={{ display: "flex", gap: 14, alignItems: "center", flexWrap: "wrap", marginBottom: 10 }}>
          <div className="mx-tabs">
            {Object.entries(TRACES).map(([k, t]) => (
              <button key={k} className="mx-tab" data-on={traceKey === k ? 1 : 0} onClick={() => setTraceKey(k)}>{t.label}</button>
            ))}
          </div>
          <div className="mx-mono" style={{ fontSize: 11.5, color: "var(--muted)", display: "flex", gap: 12, flexWrap: "wrap" }}>
            <span>cycle {ctr.cycle ?? 0}</span><span>frame {ctr.frame ?? "—"}</span>
            <span style={{ color: curPhase != null ? "var(--hot)" : "inherit" }}>phase {curPhase ?? "—"}</span>
            <span>exec {ctr.exec ?? 0}</span><span>t_sim {((ctr.simT ?? 0) * 1000).toFixed(0)} ms</span>
          </div>
          {traceKey === "tc" && (
            <div style={{ display: "flex", gap: 4, marginLeft: "auto", flexWrap: "wrap" }}>
              {PHASES.map((p) => {
                const on = curPhase === p.n;
                return (
                  <div key={p.n} className={on ? "mx-phase-now" : ""} style={{ padding: "3px 9px", borderRadius: 2, fontSize: 11.5, background: on ? "var(--ink)" : "var(--panel)", color: on ? "var(--paper)" : "var(--muted)", transition: "background 160ms, color 160ms" }}>
                    <span className="mx-mono">{p.n}</span> {p.label}
                  </div>
                );
              })}
            </div>
          )}
        </div>

        <div className="mx-graph" ref={graphRef}>
          <div className="mx-zoom">
            <div className="mx-zoomslider" title="Zoom -- também funciona com a roda do mouse, e continua valendo com 'Seguir ramo' ligado (o próprio acompanhamento move esta barra a cada passo).">
              <input type="range" min={ZOOM_MIN} max={ZOOM_MAX} step={0.01} value={view.k} aria-label="Zoom"
                onPointerDown={() => { sliderActive.current = true; }}
                onPointerUp={() => { sliderActive.current = false; }}
                onChange={(e) => setView((v) => ({ ...v, k: Number(e.target.value) }))} />
              <span className="mx-mono">{view.k.toFixed(2)}×</span>
            </div>
            <button className="mx-zbtn" data-w="1" onClick={() => setShowDetail((s) => !s)} title="Oculta o painel de detalhe abaixo, dando mais área ao grafo">
              {showDetail ? "▾ detalhe" : "▸ detalhe"}
            </button>
            <button className="mx-zbtn" data-w="1" onClick={() => setView({ k: 1, x: 0, y: 0 })}>ajustar</button>
          </div>
          <div className="mx-svgwrap" data-expanded={showDetail ? 0 : 1}>
            <svg viewBox={`${-leftMargin} ${-topMargin} ${W + (orientV ? leftMargin : 0)} ${H + topMargin}`} preserveAspectRatio="xMidYMid meet"
                 onWheel={onWheel} onPointerDown={onDown} onPointerMove={onMove} onPointerUp={onUp} onPointerLeave={onUp}>
              <g transform={`translate(${view.x},${view.y}) scale(${view.k})`}
                 style={{ transformOrigin: "center", transition: autoFollow && !drag.current && !sliderActive.current ? "transform 420ms cubic-bezier(.22,.61,.36,1)" : "none" }}>
                {["ac", "ac2"].map((pid) => {
                  const b = bandFor(pid);
                  return <rect key={pid} x={b.x0} y={b.y0} width={b.x1 - b.x0} height={b.y1 - b.y0} rx="3" fill="var(--band-bg)" />;
                })}
                {DEPTH_LABELS.map((l, d) => {
                  if (orientV) {
                    // Vertical: rótulo à ESQUERDA da linha (nunca um x fixo -- a
                    // mesma ideia de depthHeaderPos, só que ancorado à direita
                    // do texto, que cresce pra longe do nó). Divisória
                    // horizontal entre linhas, não mais vertical entre colunas.
                    const rowY = d * ROW_V;
                    const labelX = (depthHeaderPos[d] ?? 0) - 14;
                    return (
                      <g key={d}>
                        <text x={labelX} y={rowY + 3} textAnchor="end" className="mx-mono" style={{ fontSize: 10, fill: "var(--muted)" }}>{l}</text>
                        {d > 0 && <line x1={-leftMargin + 10} y1={rowY - ROW_V / 2} x2={W - 20} y2={rowY - ROW_V / 2} stroke="var(--rule)" strokeWidth="1" strokeDasharray="2 4" />}
                      </g>
                    );
                  }
                  // 12px de folga acima do no mais alto DESTA coluna -- nunca um y
                  // fixo (ver o comentario de depthHeaderPos() acima). Renderizado
                  // DEPOIS da faixa de destaque (banda ac/ac2, acima) de proposito --
                  // a banda cobre toda a subarvore de components: do Aircraft e, em
                  // ordem de pintura SVG, um elemento desenhado depois fica POR CIMA;
                  // com a ordem invertida a banda escondia por completo os rotulos de
                  // coluna que caem dentro da faixa (medido: "subsistemas"/"detalhe"/
                  // "acoes" ficavam com 100% de sobreposicao vertical, texto invisivel).
                  const labelY = (depthHeaderPos[d] ?? 0) - 12;
                  return (
                    <g key={d}>
                      <text x={d * COL} y={labelY} className="mx-mono" style={{ fontSize: 10, fill: "var(--muted)" }}>{l}</text>
                      {d > 0 && <line x1={d * COL - 20} y1={labelY - 10} x2={d * COL - 20} y2={H - 26} stroke="var(--rule)" strokeWidth="1" strokeDasharray="2 4" />}
                    </g>
                  );
                })}
                {edges.map(([a, b]) => {
                  const p = pos[a], q = pos[b], child = byId[b];
                  const hidden = child.dynamic && (!launched || vanished);
                  const onPath = pathEdges.has(a + ">" + b);
                  // A aresta que acabou de ser atravessada (termina no no ATIVO) ganha
                  // a "marcha de formigas" mais rapida, e um pulso de chegada no destino
                  // -- as demais do caminho ficam com a mesma animacao, so mais lenta.
                  const isCurrent = onPath && b === step.node;
                  // Horizontal: cotovelo direita-do-pai -> baixo/cima -> esquerda-
                  // do-filho, rótulo na goteira (largura COL-NW, sempre a mesma
                  // porque y varia por IRMÃO -- cada aresta já tem sua própria
                  // faixa vertical).
                  // Vertical: MEDIDO quebrando -- ancorar o rótulo entre pai e
                  // filho (largura = distância em x entre os dois) sobrepõe
                  // agressivamente o rótulo de QUALQUER outro filho do MESMO
                  // pai, porque todas as arestas de um fan-out largo compartilham
                  // a mesma faixa de y (o "meio" entre duas linhas de profundi-
                  // dade é igual pra todo mundo) e frequentemente a mesma faixa
                  // de x também (a do meio do pai até o proprio meio). A correção:
                  // ancorar o rótulo na largura do PRÓPRIO FILHO (mesma largura e
                  // x da caixa dele) -- como caixas de filhos nunca se sobrepõem
                  // (mesma garantia do layout()), os rótulos herdam essa garantia
                  // de graça. Cabe: o vão entre duas linhas (ROW_V-NH)/2 = 33px
                  // por lado comporta as duas linhas (via+dt, 12px cada).
                  let dPath, labelX, labelW, viaY, dtY, haloX, haloY;
                  if (orientV) {
                    const midY = (p.y + NH / 2 + (q.y - NH / 2)) / 2;
                    dPath = `M ${p.x + NW / 2} ${p.y + NH / 2} V ${midY} H ${q.x + NW / 2} V ${q.y - NH / 2}`;
                    labelX = q.x; labelW = NW;
                    viaY = midY + 2; dtY = midY + 16;
                    haloX = q.x + NW / 2; haloY = q.y - NH / 2;
                  } else {
                    const mid = p.x + NW + 16;
                    dPath = `M ${p.x + NW} ${p.y} H ${mid} V ${q.y} H ${q.x}`;
                    labelX = mid + 4; labelW = Math.max(10, q.x - (mid + 4) - 6);
                    viaY = q.y - 13; dtY = q.y + 3;
                    haloX = q.x; haloY = q.y;
                  }
                  return (
                    <g key={a + b} opacity={hidden ? 0.22 : 1}>
                      <path d={dPath} fill="none"
                        stroke={onPath ? "var(--hot)" : "var(--rule)"} strokeWidth={onPath ? 2.2 : 1}
                        className={isCurrent ? "mx-edge-current" : onPath ? "mx-edge-onpath" : ""} />
                      {isCurrent && (
                        <circle cx={haloX} cy={haloY} r="5" className="mx-halo" stroke="var(--hot)" strokeWidth="2" />
                      )}
                      {/* foreignObject+ellipsis, nao <text> livre: a goteira entre
                         * colunas (COL-NW) e finita, e rotulos de slot EDL como
                         * "dataRecorder:"/"addNewPlayer()" sao mais largos que ela --
                         * um <text> sem largura maxima cresce PRA DENTRO da caixa do
                         * no filho (pintada DEPOIS, por cima) e o excesso some sem
                         * aviso nenhum. Medido rodando: "components:" perto de
                         * Radar/AirTrkMgr aparecia cortado em "comp", com o resto
                         * escondido atras da caixa -- mesma familia de bug de #2
                         * acima, so que em texto SVG cru em vez de foreignObject. */}
                      {child.via && (
                        <foreignObject x={labelX} y={viaY} width={labelW} height="12" className="mx-fo">
                          <div className="mx-fo-row" title={child.via}>
                            <span className="mx-fo-sub" style={{ fontSize: 8.5, color: onPath ? "var(--hot)" : "var(--sub-muted)" }}>{child.via}</span>
                          </div>
                        </foreignObject>
                      )}
                      {onPath && step.dt != null && (
                        <foreignObject x={labelX} y={dtY} width={labelW} height="12" className="mx-fo">
                          <div className="mx-fo-row" title={`dt ${fmt(step.dt)}`}>
                            <span className="mx-fo-sub" style={{ fontSize: 8.5, color: "var(--hot)" }}>dt {fmt(step.dt)}</span>
                          </div>
                        </foreignObject>
                      )}
                    </g>
                  );
                })}
                {showNames && !orientV && NAME_LINKS.map((l) => {
                  const a = pos[l.from], b = pos[l.to];
                  // `l.dy` evita a MESMA faixa vertical que o "via:" do proprio no
                  // (agora sempre visivel e largo -- ver o comentario da secao via/dt
                  // acima) E evita dois links do MESMO no colidirem entre si -- ver o
                  // comentario junto de NAME_LINKS.
                  const ly = (a.y + b.y) / 2 + (l.dy || 0);
                  const lx = Math.min(a.x, b.x) - 28;
                  return (
                    <g key={l.slot} opacity="0.8">
                      <path d={`M ${a.x + NW / 2} ${a.y + NH / 2} C ${a.x - 26} ${a.y + 30}, ${b.x - 26} ${b.y - 30}, ${b.x + NW / 2} ${b.y - NH / 2}`} fill="none" stroke="var(--ok)" strokeWidth="1.3" strokeDasharray="2 3" />
                      <foreignObject x={lx - 100} y={ly - 6} width="100" height="12" className="mx-fo">
                        <div className="mx-fo-row" style={{ justifyContent: "flex-end" }} title={l.slot}>
                          <span className="mx-fo-sub" style={{ fontSize: 8.5, color: "var(--ok)" }}>{l.slot}</span>
                        </div>
                      </foreignObject>
                    </g>
                  );
                })}
                {step.kind === "rf" && (
                  <g>
                    <path d={`M ${pos.a1.x + NW / 2} ${pos.a1.y - NH / 2} C ${pos.a1.x} ${pos.a1.y - 80}, ${pos.ac2.x + NW} ${pos.ac2.y - 80}, ${pos.ac2.x + NW / 2} ${pos.ac2.y - NH / 2}`}
                      fill="none" stroke="var(--rf)" strokeWidth="1.8" strokeDasharray="5 3" className="mx-edge-evt" />
                    <circle cx={pos.ac2.x + NW / 2} cy={pos.ac2.y - NH / 2} r="5" className="mx-halo" stroke="var(--rf)" strokeWidth="2" />
                    <text x={(pos.a1.x + pos.ac2.x) / 2 + NW / 2} y={pos.a1.y - 66} textAnchor="middle" className="mx-mono" style={{ fontSize: 10, fill: "var(--rf)" }}>event(RF_EMISSION)</text>
                  </g>
                )}
                {step.kind === "release" && (
                  <g>
                    <path d={`M ${pos.sto.x + NW / 2} ${pos.sto.y + NH / 2} C ${pos.sto.x} ${pos.sto.y + 70}, ${pos.flyout.x + 30} ${pos.flyout.y - 50}, ${pos.flyout.x + NW / 2} ${pos.flyout.y - NH / 2}`}
                      fill="none" stroke="var(--new)" strokeWidth="2" strokeDasharray="4 3" className="mx-edge-evt" />
                    <circle cx={pos.flyout.x + NW / 2} cy={pos.flyout.y - NH / 2} r="5" className="mx-halo" stroke="var(--new)" strokeWidth="2" />
                  </g>
                )}
                {nodes.map((n) => {
                  const active = step.node === n.id;
                  const inStack = (step.stack || []).some((s) => s.node === n.id);
                  const running = active && step.runs;
                  const ghost = n.dynamic && (!launched || vanished);
                  const never = !n.phases.length;
                  const v = visits[n.id] || 0;
                  // Key composta SO nos nos dinamicos (o missil): forca remontagem quando
                  // ghost vira real (e vice-versa), o que faz a animacao de entrada
                  // (mx-popin) tocar de novo -- sem isso, o React so atualiza atributos
                  // do MESMO elemento e a keyframe nunca reinicia.
                  const key = n.dynamic ? `${n.id}-${ghost ? "g" : "r"}` : n.id;
                  return (
                    <g key={key} className="mx-node" data-pop={n.dynamic && !ghost ? 1 : 0}
                       transform={`translate(${n.x},${n.y - NH / 2})`}
                       onClick={(e) => {
                         setPlaying(false);
                         const willPin = pinned !== n.id;
                         setPinned(willPin ? n.id : null);
                         if (willPin && graphRef.current) {
                           const r = graphRef.current.getBoundingClientRect();
                           setPopup({ nodeId: n.id, x: e.clientX - r.left, y: e.clientY - r.top });
                         } else {
                           setPopup(null);
                         }
                       }}>
                      {running && <circle cx={NW / 2} cy={NH / 2} r={NH / 2} className="mx-halo" stroke="var(--hot)" strokeWidth="2.5" />}
                      <rect x="0" y="0" width={NW} height={NH} rx="2"
                        fill={running ? "var(--hot)" : active ? "var(--active-bg)" : never ? "var(--never-bg)" : "var(--paper)"}
                        stroke={pinned === n.id ? "var(--ink)" : running ? "var(--hot)" : active ? "var(--ink)" : inStack ? "var(--muted)" : "var(--rule)"}
                        strokeWidth={active || pinned === n.id ? 1.6 : 1}
                        strokeDasharray={ghost ? "3 2" : "0"} opacity={ghost ? 0.45 : 1} />
                      <rect x="0" y="0" width="3" height={NH} fill={THREAD_COLOR[n.thread] || "var(--rule)"} opacity={ghost ? 0.4 : 0.9} />
                      <foreignObject x="9" y="2" width={NW - 58} height="16" className="mx-fo" style={{ opacity: ghost ? 0.55 : 1 }}>
                        <div className="mx-fo-row" title={n.cls}><span className="mx-fo-cls" style={{ fontSize: 11, color: running ? "var(--paper)" : never ? "var(--muted)" : "var(--ink)" }}>{n.cls}</span></div>
                      </foreignObject>
                      {/* largura igual a do nome (NW-58): a faixa dos pips de fase   *
                         * (comeca em NW-48, y=21) cai bem NESSA linha -- um subtitulo *
                         * mais largo que isso ficava por baixo dos pips (medido     *
                         * rodando em zoom alto: "ownship .player .s/System" cobria   *
                         * os 3 primeiros quadradinhos de fase). */}
                      <foreignObject x="9" y="18" width={NW - 58} height="12" className="mx-fo" style={{ opacity: ghost ? 0.5 : 1 }}>
                        <div className="mx-fo-row" title={`${n.edl}${n.player ? " ·player" : ""}${n.disp ? "" : " ·s/System"}`}><span className="mx-fo-sub" style={{ fontSize: 9, color: running ? "var(--running-fg)" : "var(--sub-muted)" }}>
                          {n.edl}{n.player ? " ·player" : ""}{n.disp ? "" : " ·s/System"}
                        </span></div>
                      </foreignObject>
                      <g transform={`translate(${NW - 48}, 21)`} opacity={ghost ? 0.5 : 1}>
                        {PHASES.map((p) => {
                          const has = n.phases.includes(p.n);
                          const own = has && phaseOwner(n.cls, p.n) === n.cls;
                          const now = has && curPhase === p.n;
                          return <rect key={p.n} x={p.n * 10} y="0" width="7" height="7" rx="1" className={now ? "mx-phase-now" : ""}
                            fill={now ? (running ? "var(--phase-now-bg)" : "var(--hot)") : has ? (running ? "var(--phase-has-running-bg)" : own ? "var(--ink)" : "var(--phase-inherited)") : "none"}
                            stroke={has ? "none" : running ? "var(--phase-stroke-running)" : "var(--rule)"} strokeWidth="1" />;
                        })}
                      </g>
                      {v > 0 && <text x={NW - 7} y="14" textAnchor="end" className="mx-mono" style={{ fontSize: 9, fill: running ? "var(--running-fg)" : "var(--muted)" }}>×{v}</text>}
                    </g>
                  );
                })}
              </g>
            </svg>
          </div>
          {popup && byId[popup.nodeId] && (() => {
            const n = byId[popup.nodeId];
            const e = MODEL[n.cls] || {};
            const fname = factoryOf(n.cls);
            const diverges = fname !== n.cls;
            const gw = graphRef.current ? graphRef.current.clientWidth : 800;
            const gh = graphRef.current ? graphRef.current.clientHeight : 500;
            const PW = 250, PH = 172;
            const left = Math.min(Math.max(8, popup.x + 14), Math.max(8, gw - PW - 8));
            const top = Math.min(Math.max(40, popup.y - 12), Math.max(40, gh - PH - 8));
            return (
              <div className="mx-nodepopup" style={{ left, top, width: PW }}>
                <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline", gap: 6 }}>
                  <span className="mx-mono" style={{ fontWeight: 600, fontSize: 12.5 }}>{n.cls}</span>
                  <button className="mx-nodepopup-x" onClick={() => setPopup(null)} aria-label="Fechar" title="Fechar">×</button>
                </div>
                <div style={{ fontSize: 11.5, lineHeight: 1.6, marginTop: 5 }}>
                  <div>nome de fábrica: <b className="mx-mono">{fname}</b>{diverges && (
                    <span style={{ color: "var(--hot)" }} title="Nome de fábrica diferente do nome da classe C++ -- é ESTE que o EDL espera dentro de ( ... )">
                      {" "}≠ classe C++
                    </span>
                  )}</div>
                  <div>registrada em fábrica: {e.r
                    ? "sim"
                    : <b style={{ color: "var(--rf)" }} title="Sem IMPLEMENT_*SUBCLASS -- um ( ... ) com este nome no EDL falha com 'unknown factory name'">não</b>}</div>
                  <div>slots próprios: {e.sl ? e.sl.length : 0}{e.b && <> · deriva de <span className="mx-mono">{e.b}</span></>}</div>
                </div>
                {onOpenCatalog && (
                  <button className="mx-nodepopup-link" onClick={() => onOpenCatalog(n.cls)}>Ver classe completa no Catálogo →</button>
                )}
              </div>
            );
          })()}
          <div className="mx-leg">
            <button className="mx-leg-toggle" onClick={() => setShowCardLegend((s) => !s)}>
              {showCardLegend ? "▾" : "▸"} como ler um cartão
            </button>
            <span><b style={{ color: "var(--hot)" }}>■</b> executando</span>
            <span>pips: <b>■</b> implementa · <b style={{ color: "var(--phase-inherited)" }}>■</b> herda de ancestral · ▫ ninguém na cadeia</span>
            <span>×n visitas</span>
            <span>thread: <b style={{ color: "var(--ink)" }}>TC</b> · <b style={{ color: "var(--bgc)" }}>fundo</b> · <b style={{ color: "var(--ok)" }}>rede</b></span>
            <span style={{ color: "var(--rf)" }}>--- evento</span>
            <span style={{ color: "var(--ok)" }}>··· por nome</span>
            <span>roda = zoom · arrastar = mover</span>
          </div>
          {/* Legenda visual: pedido explicito ("talvez uma legenda ajude
             * muito") -- a linha de texto acima diz O QUE cada marca significa,
             * mas nao ONDE ela fica no cartao. Um cartao de exemplo, anotado com
             * setas, responde "rápido entendimento" melhor que prosa -- fechado
             * por padrao pra nao competir por altura com o grafo (o mesmo motivo
             * do toggle "detalhe"). */}
          {showCardLegend && (
            <div className="mx-cardleg">
              {/* viewBox bem mais largo que o cartão em si: cada rótulo precisa
                 * de ~190px, e a goteira esquerda/direita do cartão de exemplo
                 * tem de caber isso INTEIRO -- a mesma lição do achado sobre os
                 * rótulos "via:"/"dt:" do grafo principal (goteira estreita
                 * corta texto por baixo da caixa). Aqui não há caixa vizinha
                 * pra esconder o corte, então o risco é pior: o texto simplesmente
                 * sai do viewBox e desaparece, sem nem um "..." de aviso. */}
              <svg viewBox="0 0 900 220" width="900" height="220">
                <g transform="translate(350,30)">
                  <rect x="0" y="0" width={NW} height={NH} rx="2" fill="var(--paper)" stroke="var(--ink)" strokeWidth="1.6" />
                  <rect x="0" y="0" width="3" height={NH} fill="var(--bgc)" />
                  <text x="9" y="13" className="mx-mono" style={{ fontSize: 11, fontWeight: 600, fill: "var(--ink)" }}>Radar</text>
                  <text x="9" y="27" className="mx-mono" style={{ fontSize: 9, fill: "var(--sub-muted)" }}>radar ·tc+fundo</text>
                  <g transform={`translate(${NW - 48}, 21)`}>
                    {[0, 1, 2, 3].map((p) => (
                      <rect key={p} x={p * 10} y="0" width="7" height="7" rx="1"
                        fill={p < 2 ? "var(--ink)" : "none"} stroke={p < 2 ? "none" : "var(--rule)"} strokeWidth="1" />
                    ))}
                  </g>
                  <text x={NW - 7} y="14" textAnchor="end" className="mx-mono" style={{ fontSize: 9, fill: "var(--muted)" }}>×3</text>
                </g>
                {[
                  // Ancora sempre na BORDA do cartao (nunca em cima de um glifo
                  // especifico) -- version anterior colocava o ponto exatamente
                  // sobre o "3" de "x3" e sobre um pip, competindo visualmente
                  // com o proprio conteudo que a legenda tenta explicar.
                  { x: 350, y: 39, lx: 210, ly: 18, w: 190, align: "right", label: "nome da classe C++ (nunca o nome de fábrica — veja a aba Classe)" },
                  { x: 350, y: 53, lx: 210, ly: 112, w: 190, align: "right", label: "identificador no EDL — mais \"·player\" ou \"·s/System\" quando aplicável" },
                  { x: 351, y: 47, lx: 210, ly: 178, w: 190, align: "right", label: "cor = onde a classe roda: preta TC, azul fundo, verde rede" },
                  { x: 558, y: 44, lx: 700, ly: 130, w: 190, align: "left", label: "4 quadrados = as 4 fases do frame; preenchido = implementa esta fase (herdada ou própria)" },
                  { x: 551, y: 30, lx: 700, ly: 20, w: 190, align: "left", label: "×N — quantas vezes este nó já foi visitado até o passo atual" },
                ].map((c, k) => (
                  <g key={k}>
                    <line x1={c.x} y1={c.y} x2={c.lx} y2={c.ly} stroke="var(--muted)" strokeWidth="1" strokeDasharray="2 2" />
                    <circle cx={c.x} cy={c.y} r="2.5" fill="var(--muted)" />
                    <foreignObject x={c.align === "right" ? c.lx - c.w : c.lx} y={c.ly - 9} width={c.w} height="46" className="mx-fo">
                      <div style={{ fontSize: 10.5, lineHeight: 1.3, color: "var(--muted)", textAlign: c.align }}>{c.label}</div>
                    </foreignObject>
                  </g>
                ))}
              </svg>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "4px 0 0", maxWidth: 720 }}>
                A borda fica <b style={{ color: "var(--ink)" }}>tracejada</b> quando o nó ainda não existe (um míssil na
                estação, antes do lançamento) e some o tracejado assim que ele nasce como player de verdade. O nó
                fica <b style={{ color: "var(--hot)" }}>laranja</b>, com um halo pulsando, exatamente no passo em que ele está EXECUTANDO
                agora — os demais na pilha de chamadas (acima dele) ficam só com a borda mais escura.
              </p>
            </div>
          )}
        </div>

        {showDetail && (
        <div className="mx-pane">
          {/* Clicar num cartão do grafo pausa (ver onClick de .mx-node) e fixa   *
             * este nó -- o painel abaixo (EDL/Classe/Código) já segue o fixado, *
             * não o passo atual; este bloco só torna isso EXPLÍCITO, com um    *
             * dado que não muda ao avançar/voltar o passo (ao contrário do     *
             * "×n" no próprio cartão, que conta só até aqui). */}
          {pinned && byId[pinned] && (
            <div className="mx-card" style={{ marginBottom: 10, borderLeft: "3px solid var(--hot)" }}>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline", gap: 8, flexWrap: "wrap" }}>
                <span className="mx-mono" style={{ fontWeight: 600, fontSize: 12.5 }}>📌 fixado — {byId[pinned].cls}</span>
                <button className="mx-btn" style={{ fontSize: 11, padding: "2px 8px" }} onClick={() => { setPinned(null); setPopup(null); }}>soltar</button>
              </div>
              <div style={{ fontSize: 11.5, color: "var(--muted)", marginTop: 4 }}>
                Não muda ao avançar/voltar o passo — thread <b>{byId[pinned].thread}</b>.{" "}
                {(() => {
                  const n = trace.filter((s) => s.node === pinned).length;
                  return n > 0
                    ? <>Aparece em <b className="mx-mono">{n}</b> de <b className="mx-mono">{trace.length}</b> passos da trilha "{TRACES[traceKey].label}".</>
                    : <>Não é visitado pela trilha "{TRACES[traceKey].label}" — ver Classe/EDL abaixo para os dados estáticos.</>;
                })()}
              </div>
            </div>
          )}
          <div className="mx-dtabs" role="tablist" aria-label="Detalhe do passo">
            <button className="mx-dtab" role="tab" aria-selected={detailTab === "step"} data-on={detailTab === "step" ? 1 : 0} onClick={() => setDetailTab("step")}>Passo</button>
            <button className="mx-dtab" role="tab" aria-selected={detailTab === "code"} data-on={detailTab === "code" ? 1 : 0} disabled={!snip} onClick={() => snip && setDetailTab("code")}>
              Código{snip && step.hl ? <span className="mx-dtab-dot" /> : null}
            </button>
            <button className="mx-dtab" role="tab" aria-selected={detailTab === "edl"} data-on={detailTab === "edl" ? 1 : 0} onClick={() => setDetailTab("edl")}>EDL do cenário</button>
            <button className="mx-dtab" role="tab" aria-selected={detailTab === "class"} data-on={detailTab === "class" ? 1 : 0} onClick={() => setDetailTab("class")}>Classe</button>
          </div>

          <div className="mx-detailbody" key={detailTab}>
            {detailTab === "step" && (
              <>
                <div className="mx-card">
                  <div className="mx-mono" style={{ fontSize: 12.5, fontWeight: 600, marginBottom: 5 }}>{step.title}</div>
                  <p style={{ margin: 0, fontSize: 13, lineHeight: 1.5 }}>{step.body}</p>
                  {step.warn && <p className="mx-warn">{step.warn}</p>}
                </div>
                <div className="mx-lbl" style={{ marginTop: 12 }}><span>Pilha de chamadas</span></div>
                {(step.stack || []).map((s, k) => (
                  <div key={k} className="mx-mono" style={{ fontSize: 11.5, padding: "2px 0 2px 9px", marginLeft: k * 7, borderLeft: `2px solid ${k === step.stack.length - 1 ? "var(--hot)" : "var(--rule)"}` }}>{s.label}</div>
                ))}
              </>
            )}

            {detailTab === "code" && snip && (
              <>
                <div className="mx-lbl">
                  <span className="mx-mono">{snip.file}:{snip.line + (step.hl ? step.hl[0] : 0)}</span>
                  <span>{snip.trunc ? "corpo truncado na extração" : "C++"}</span>
                </div>
                {/* SEM caixa de rolagem: codeWin ja e a janela de linhas (no maximo   *
                   * 22) centrada no trecho destacado -- ver windowLines(). */}
                <div className="mx-code">
                  {codeWin.cutBefore && <div className="mx-codecut">⋯ {codeWin.offset} linha{codeWin.offset === 1 ? "" : "s"} acima ⋯</div>}
                  {codeWin.lines.map((ln, k) => {
                    const abs = k + codeWin.offset;
                    const on = step.hl && abs >= step.hl[0] && abs <= step.hl[1];
                    return <div key={abs} className="mx-cl" data-on={on ? 1 : 0}><span className="mx-num">{snip.line + abs}</span><span className="mx-src">{ln || " "}</span></div>;
                  })}
                  {codeWin.cutAfter && <div className="mx-codecut">⋯ {snip.lines.length - codeWin.offset - codeWin.lines.length} linhas abaixo ⋯</div>}
                </div>
              </>
            )}

            {detailTab === "edl" && (
              <>
                <div className="mx-lbl"><span className="mx-mono">{ubfEdlRange ? "ilustrativo (não é o EDL de produção)" : "cenario.edl"}</span><span>{detail.cls} · {detail.edl}</span></div>
                <div className="mx-edl">
                  {edlWin.cutBefore && <div className="mx-codecut">⋯ {edlWin.offset} linha{edlWin.offset === 1 ? "" : "s"} acima ⋯</div>}
                  {edlWin.lines.map((ln, k) => {
                    const abs = k + edlWin.offset;
                    const on = abs >= edlRange[0] && abs <= edlRange[1];
                    return <div key={abs} className="mx-cl" data-on={on ? 1 : 0}><span className="mx-num">{abs + 1}</span><span className="mx-src">{ln || " "}</span></div>;
                  })}
                  {edlWin.cutAfter && <div className="mx-codecut">⋯ {edlSrc.length - edlWin.offset - edlWin.lines.length} linhas abaixo ⋯</div>}
                </div>
              </>
            )}

            {detailTab === "class" && (
              <>
                <div className="mx-lbl">
                  <span className="mx-mono" style={{ color: "var(--ink)", fontWeight: 600 }}>{detail.cls}</span>
                  <span>{pinned ? "fixado" : "segue a execução"}</span>
                </div>
                <div style={{ fontSize: 11.5, color: "var(--muted)", marginBottom: 7 }}>
                  módulo <b className="mx-mono">{dm.m}</b> · EDL <b className="mx-mono">( {factoryOf(detail.cls)} )</b>
                  {dm.f ? <span style={{ color: "var(--rf)" }}> · nome divergente</span> : null}
                  {dm.r === false ? <span style={{ color: "var(--rf)" }}> · NÃO registrada</span> : null}
                  <br />{dm.src || dm.hd}
                </div>
                {chainOf(detail.cls).map((c, k) => {
                  const ph = (MODEL[c] && MODEL[c].sl ? MODEL[c].sl.length : 0);
                  const own = PHASES.filter((p) => (MODEL[c] ? MODEL[c].wp : []).length && phaseOwner(detail.cls, p.n) === c);
                  return (
                    <div key={c} style={{ padding: "3px 8px", marginLeft: k * 6, borderLeft: `2px solid ${own.length ? "var(--hot)" : "var(--rule)"}`, background: own.length ? "var(--panel)" : "transparent" }}>
                      <span className="mx-mono" style={{ fontSize: 11.5, fontWeight: own.length ? 600 : 400 }}>{c}</span>
                      <span style={{ fontSize: 11, color: "var(--muted)" }}>
                        {own.length ? ` — ${own.map((p) => p.m + "()").join(", ")}` : ""}
                        {ph ? ` · ${ph} slots` : ""}
                      </span>
                    </div>
                  );
                })}
                <div className="mx-lbl" style={{ marginTop: 12 }}>
                  <span>Slots ({slots.length} na cadeia)</span><span>{dm.own || 0} próprios</span>
                </div>
                <div className="mx-slotgrid">
                  {slots.map(([s, from], k) => (
                    <div className="mx-slot" key={s + k}><span>{s}</span><span>{from}</span></div>
                  ))}
                  {!slots.length && <div style={{ fontSize: 11.5, color: "var(--muted)" }}>Nenhum slot em toda a cadeia.</div>}
                </div>
                {detail.note && <p className="mx-warn">{detail.note}</p>}
              </>
            )}
          </div>
        </div>
        )}
      </div>

      <div className="mx-transport" ref={transportRef}>
        <button className="mx-btn" data-primary="1" onClick={() => setPlaying((p) => !p)}>
          {playing && <span className="mx-dtab-dot" style={{ marginRight: 6, animation: "mx-glow 900ms ease-in-out infinite" }} />}
          {playing ? "Pausar" : "Reproduzir"}
        </button>
        <button className="mx-btn" onClick={() => move(-1)}>←</button>
        <button className="mx-btn" onClick={() => move(1)}>→</button>
        <button className="mx-btn" onClick={() => { setPlaying(false); setI(0); }}>Início</button>
        <div className="mx-tl" role="slider" aria-label="Linha do tempo" aria-valuenow={idx} aria-valuemin={0} aria-valuemax={trace.length - 1} tabIndex={0}
             onKeyDown={(e) => { if (e.key === "ArrowRight") move(1); if (e.key === "ArrowLeft") move(-1); }}>
          {trace.map((s, k) => {
            const evt = ["rf", "release", "name", "phase"].includes(s.kind);
            return <div key={k} className="mx-seg" onClick={() => { setPlaying(false); setI(k); }} title={s.title}
              style={{ background: k === idx ? "var(--hot)" : segColor(s), height: k === idx ? "100%" : evt ? "70%" : "40%", opacity: k <= idx ? 1 : 0.4 }} />;
          })}
        </div>
        <span className="mx-mono" style={{ fontSize: 11.5, color: "var(--muted)", minWidth: 52 }}>{idx + 1}/{trace.length}</span>
        <label style={{ fontSize: 12.3, display: "flex", gap: 5, alignItems: "center" }}>
          <input type="checkbox" checked={showIdle} onChange={(e) => setShowIdle(e.target.checked)} /> Ociosos
        </label>
        <label style={{ fontSize: 12.3, display: "flex", gap: 5, alignItems: "center" }}
               title={orientV ? "Indisponível na árvore vertical (as setas pontilhadas ainda não têm posição calibrada nesse layout)" : ""}>
          <input type="checkbox" checked={showNames && !orientV} disabled={orientV} onChange={(e) => setShowNames(e.target.checked)} /> Ligações por nome
        </label>
        <label style={{ fontSize: 12.3, display: "flex", gap: 5, alignItems: "center" }} title="Zoom/pan acompanham sozinhos o ramo em destaque a cada passo">
          <input type="checkbox" checked={autoFollow} onChange={(e) => setAutoFollow(e.target.checked)} /> Seguir ramo
        </label>
        <label style={{ fontSize: 12.3, display: "flex", gap: 5, alignItems: "center" }} title="Raiz em cima, irmãos lado a lado, em vez de raiz à esquerda">
          <input type="checkbox" checked={orientV} onChange={(e) => setOrientation(e.target.checked ? "v" : "h")} /> Árvore vertical
        </label>
        <select className="mx-input" value={speed} onChange={(e) => setSpeed(Number(e.target.value))} aria-label="Velocidade">
          <option value={1100}>Lento</option><option value={650}>Normal</option><option value={240}>Rápido</option>
        </select>
      </div>
    </>
  );
}

/* ----------------------------- eventos ------------------------------ */

// Aba dedicada SÓ a ilustrar os quatro momentos "kind" especiais que já
// existem dentro da trilha "Thread de Tempo Crítico"/"Reset" (ver
// eventTour() acima) -- o pedido explícito foi por uma aba À PARTE, porque
// misturados aos ~258 passos rotineiros da trilha principal esses quatro
// são fáceis de nunca notar. Deliberadamente NÃO é o grafo pan/zoom
// completo (isso já existe em Execução): cada evento é uma ponte entre DOIS
// pontos da árvore que não têm relação de pai-filho nenhuma, então o que
// importa mostrar é de ONDE cada lado desce (a cadeia raiz->nó) e O QUE
// liga os dois -- um "breadcrumb" simples em HTML/CSS já basta, sem
// reintroduzir viewBox/pan/zoom/drag pra só dois nós de cada vez.
function EventCrumb({ chain, highlightColor }) {
  return (
    <div style={{ display: "flex", flexWrap: "wrap", alignItems: "center", gap: 4, fontSize: 11.5 }}>
      {chain.map((n, i) => {
        const last = i === chain.length - 1;
        return (
          <span key={n.id} style={{ display: "flex", alignItems: "center", gap: 4 }}>
            <span className="mx-mono" style={{
              padding: "2px 7px", borderRadius: 2,
              background: last ? highlightColor : "var(--panel)",
              color: last ? "var(--paper)" : "var(--ink)",
              fontWeight: last ? 600 : 400,
            }}>{n.cls}</span>
            {!last && <span style={{ color: "var(--muted)" }}>›</span>}
          </span>
        );
      })}
    </div>
  );
}

function Events() {
  const tour = useMemo(() => eventTour(), []);
  const [sel, setSel] = useState(0);
  const idx = Math.min(sel, tour.length - 1);
  const ev = tour[idx];
  const color = EVENT_KIND_COLOR[ev.kind] || "var(--ink)";

  useEffect(() => {
    const h = (e) => {
      if (e.key === "ArrowRight") setSel((p) => Math.min(tour.length - 1, p + 1));
      else if (e.key === "ArrowLeft") setSel((p) => Math.max(0, p - 1));
    };
    window.addEventListener("keydown", h);
    return () => window.removeEventListener("keydown", h);
  }, [tour.length]);

  // A direção da seta é "quem SEGURA o ponteiro" -> "quem é achado por ele" --
  // não é sempre ev.node -> ev.to: em "name" é o INVERSO. O push de "name"
  // (em walk(), no turno de visita do PRÓPRIO track manager) marca
  // node="ttm" (onde o passo acontece) e from="tws" (quem guarda o
  // trackManagerName) -- mas quem referencia quem, de verdade, é tws
  // (o slot mora nele) apontando PARA ttm, não o contrário. "rf" (node=
  // antena emissora, to=alvo) e "release" (node=Stores liberador, to=novo
  // player) já nascem na ordem certa: node é sempre a origem nesses dois.
  const [srcId, dstId] = ev.kind === "name" ? [ev.from, ev.node]
    : ev.kind === "vanish" ? [ev.node, null]
    : [ev.node, ev.to];
  const fromChain = pathChain(srcId);
  const toChain = dstId ? pathChain(dstId) : null;

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <p style={{ fontSize: 13, lineHeight: 1.6, color: "var(--muted)", maxWidth: 900, margin: "0 0 16px" }}>
        Quatro momentos em que o PRÓXIMO passo não é filho do nó atual — pontes que nenhuma
        leitura estática da árvore de <code className="mx-mono">components:</code> revela.
        A trilha "Thread de Tempo Crítico" (e "Reset") já passa por eles, um por vez, entre
        dezenas de passos de visita rotineira; aqui são os quatro sozinhos, para ver a
        estrutura da ponte sem precisar achá-la no meio do resto.
      </p>

      <div style={{ display: "flex", gap: 8, flexWrap: "wrap", marginBottom: 16 }}>
        {tour.map((t, i) => (
          <button key={t.kind} className="mx-tab" data-on={i === idx ? 1 : 0} onClick={() => setSel(i)}
                  style={{ borderLeft: `3px solid ${EVENT_KIND_COLOR[t.kind] || "var(--rule)"}` }}>
            {EVENT_KIND_LABEL[t.kind] || t.kind}
          </button>
        ))}
      </div>

      <div className="mx-card" style={{ maxWidth: 900 }}>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline", gap: 8, flexWrap: "wrap" }}>
          <span className="mx-mono" style={{ fontWeight: 600, fontSize: 13.5, color }}>{ev.title}</span>
          <span className="mx-mono" style={{ fontSize: 11, color: "var(--muted)" }}>{ev.src}</span>
        </div>

        <div style={{ margin: "14px 0", display: "flex", flexDirection: "column", gap: 8 }}>
          <EventCrumb chain={fromChain} highlightColor={color} />
          {toChain && (
            <>
              <div style={{ marginLeft: 8, color, fontSize: 15, lineHeight: 1 }}>↓</div>
              <EventCrumb chain={toChain} highlightColor={color} />
            </>
          )}
        </div>

        <p style={{ margin: "0 0 8px", fontSize: 13, lineHeight: 1.55 }}>{ev.body}</p>
        {ev.warn && <p className="mx-warn">{ev.warn}</p>}
        <p className="mx-warn" style={{ borderLeftColor: color }}>{EVENT_KIND_NOTE[ev.kind]}</p>
      </div>

      <div style={{ display: "flex", gap: 8, marginTop: 16 }}>
        <button className="mx-btn" disabled={idx === 0} onClick={() => setSel((p) => Math.max(0, p - 1))}>← anterior</button>
        <button className="mx-btn" disabled={idx === tour.length - 1} onClick={() => setSel((p) => Math.min(tour.length - 1, p + 1))}>próximo →</button>
        <span className="mx-mono" style={{ fontSize: 11.5, color: "var(--muted)", alignSelf: "center" }}>{idx + 1}/{tour.length}</span>
      </div>
    </div>
  );
}

/* ---------------------------- catálogo ----------------------------- */

const MOD_ORDER = ["base", "simulation", "terrain", "linkage", "recorder", "models", "interop/dis", "interop/rprfom"];

function Catalog({ onOpen, openClass, setOpenClass }) {
  const [q, setQ] = useState("");
  const [filter, setFilter] = useState("all");
  const [sel, setSel] = useState(null);
  const boxRef = useRef(null);

  useEffect(() => {
    const h = (e) => { if (e.key === "/" && document.activeElement !== boxRef.current) { e.preventDefault(); boxRef.current && boxRef.current.focus(); } };
    window.addEventListener("keydown", h);
    return () => window.removeEventListener("keydown", h);
  }, []);

  // Chegada vinda do popup de nó da aba Execução ("ver classe completa no
  // Catálogo") -- mesmo padrão do useEffect de `focus` dentro de Exec:
  // consome o pedido (volta a null) pra um clique repetido na MESMA classe
  // reabrir o cartão de novo.
  useEffect(() => {
    if (!openClass) return;
    setSel(openClass);
    setOpenClass(null);
  }, [openClass, setOpenClass]);

  const match = (c) => {
    const e = MODEL[c]; if (!e) return false;
    const t = q.trim().toLowerCase();
    if (t && !(c.toLowerCase().includes(t) || (e.f || "").toLowerCase().includes(t) ||
      (e.sl || []).some((s) => s.toLowerCase().includes(t)))) return false;
    if (filter === "div" && !e.f) return false;
    if (filter === "unreg" && e.r) return false;
    if (filter === "phase" && !e.wp.length) return false;
    if (filter === "scn" && !IN_SCENARIO.has(c)) return false;
    if (filter === "ubf" && !chainOf(c).some((a) => UBF_ROOTS.includes(a))) return false;
    return true;
  };

  const mods = useMemo(() => {
    const byMod = {};
    Object.entries(FACTORIES).forEach(([mod, d]) => { byMod[mod] = { file: d.file, list: d.classes.filter(match) }; });
    return byMod;
  }, [q, filter]);

  const unregistered = useMemo(
    () => Object.keys(MODEL).filter((c) => !MODEL[c].r && match(c)).sort(),
    [q, filter]);

  const shownCount = Object.values(mods).reduce((a, m) => a + m.list.length, 0) + (filter === "unreg" ? 0 : 0);

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <div className="mx-stats">
        <span><b>{STATS.classes}</b> classes com DECLARE_SUBCLASS</span>
        <span><b>{STATS.registered}</b> registradas em fábrica</span>
        <span><b>{STATS.divergent}</b> com nome de fábrica divergente</span>
        <span><b>{STATS.slotsTotal}</b> slots em <b>{STATS.withSlots}</b> classes</span>
        <span><b>{STATS.dispatch}</b> despacham por fase (derivam de System)</span>
        <span><b>{STATS.phaseWork}</b> fazem trabalho em alguma fase — <b>{STATS.phaseOwn}</b> a implementam de fato</span>
      </div>

      <div style={{ display: "flex", gap: 10, alignItems: "center", flexWrap: "wrap", marginBottom: 8 }}>
        <input ref={boxRef} className="mx-input" type="text" value={q} onChange={(e) => setQ(e.target.value)}
          placeholder="Classe, nome de fábrica ou slot  ( / )" style={{ minWidth: 240, flex: "0 1 340px" }} aria-label="Filtrar" />
        <div className="mx-tabs">
          {[["all", "Todas"], ["div", "Nome divergente"], ["phase", "Trabalha em fase"], ["unreg", "Não registradas"], ["scn", "No cenário"], ["ubf", "Decisão (UBF)"]].map(([k, l]) => (
            <button key={k} className="mx-tab" data-on={filter === k ? 1 : 0} onClick={() => setFilter(k)}>{l}</button>
          ))}
        </div>
        <span style={{ fontSize: 12, color: "var(--muted)" }}>{filter === "unreg" ? unregistered.length : shownCount} resultados</span>
      </div>

      {sel && <ClassCard c={sel} onClose={() => setSel(null)} onOpen={onOpen} />}

      {filter !== "unreg" && MOD_ORDER.filter((m) => mods[m] && mods[m].list.length).map((m) => (
        <div key={m} className="mx-mod">
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline", gap: 12, flexWrap: "wrap" }}>
            <h2 className="mx-mono" style={{ fontSize: 13.5, margin: 0, fontWeight: 600 }}>{mods[m].file}</h2>
            <span className="mx-chip">{FACTORIES[m].classes.length} nomes registrados</span>
          </div>
          <div className="mx-wrap" style={{ marginTop: 8 }}>
            {mods[m].list.map((c) => <ClassChip key={c} c={c} onClick={() => setSel(c)} />)}
          </div>
        </div>
      ))}

      <div className="mx-mod">
        <h2 className="mx-mono" style={{ fontSize: 13.5, margin: 0, fontWeight: 600 }}>
          Declaradas no fonte, não registradas em nenhuma fábrica ({Object.keys(MODEL).filter((c) => !MODEL[c].r).length})
        </h2>
        <p style={{ fontSize: 12.5, color: "var(--muted)", margin: "3px 0 8px", maxWidth: 840 }}>
          Têm <span className="mx-mono">DECLARE_SUBCLASS</span> e muitas têm nome de fábrica declarado, mas nenhum{" "}
          <span className="mx-mono">factory.cpp</span> as constrói. Escrevê-las em EDL não produz erro — produz nada.
        </p>
        <div className="mx-wrap">
          {unregistered.map((c) => <ClassChip key={c} c={c} onClick={() => setSel(c)} />)}
        </div>
      </div>
    </div>
  );
}

function ClassChip({ c, onClick }) {
  const e = MODEL[c] || {};
  return (
    <span className="mx-cls" data-scn={IN_SCENARIO.has(c) ? 1 : 0} data-div={e.f ? 1 : 0} data-reg={e.r ? 1 : 0}
      onClick={onClick} title={`${c} < ${e.b || "—"}`}>
      {c}
      {e.f && <em style={{ color: "var(--rf)", fontStyle: "normal", fontSize: 10.5 }}>→{e.f}</em>}
      {e.wp && e.wp.length ? <span style={{ color: "var(--hot)", fontSize: 10 }}>{e.wp.join("")}</span> : null}
    </span>
  );
}

function ClassCard({ c, onClose, onOpen }) {
  const e = MODEL[c]; if (!e) return null;
  const slots = allSlots(c);
  const derived = Object.keys(MODEL).filter((k) => MODEL[k].b === c);
  // Só existe corpo real pros métodos que as trilhas de Execução já citam
  // (SNIPPETS é curadoria manual, não as 342 classes inteiras). Varre as
  // CHAVES de SNIPPETS por prefixo "Classe::" em vez de cruzar com e.ov
  // (a lista de overrides que o MODEL embutido guardou): e.ov só cobre os 7
  // métodos de fase originais na hora em que MODEL foi gerado -- controller/
  // genAction/updateState/execute/genComplexAction (acrescentados depois a
  // TARGET_METHODS, ver scripts/extract_execution_chain.py) já têm corpo
  // real em SNIPPETS mas não em e.ov, que não foi regenerado. Varrer por
  // prefixo é imune a essa defasagem -- e também cobre método que a classe
  // DECLARA pela primeira vez (não "sobrescreve" nada), caso de Agent::
  // controller. Só filhos DIRETOS aparecem (própria classe, não herdados) --
  // a cadeia continua navegável pelo próprio card.
  const ownSnippets = Object.keys(SNIPPETS)
    .filter((k) => k.startsWith(`${c}::`))
    .map((k) => [k.slice(c.length + 2), SNIPPETS[k]]);
  return (
    <div className="mx-card" style={{ margin: "8px 0 6px" }}>
      <div style={{ display: "flex", justifyContent: "space-between", gap: 12, flexWrap: "wrap" }}>
        <div>
          <span className="mx-mono" style={{ fontWeight: 600, fontSize: 14 }}>{c}</span>
          <span className="mx-mono" style={{ color: "var(--muted)", fontSize: 12 }}> · EDL ( {e.f || c} ){e.r ? "" : " · NÃO registrada"}</span>
          <div className="mx-mono" style={{ fontSize: 11, color: "var(--muted)" }}>{e.src || e.hd}</div>
        </div>
        <div style={{ display: "flex", gap: 6 }}>
          {IN_SCENARIO.has(c) && <button className="mx-btn" onClick={() => onOpen(c)}>ver no cenário</button>}
          <button className="mx-btn" onClick={onClose}>fechar</button>
        </div>
      </div>

      <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fit, minmax(240px, 1fr))", gap: 14, marginTop: 10 }}>
        <div>
          <div className="mx-lbl"><span>Cadeia de herança</span></div>
          {e.ch.map((a, k) => (
            <div key={a} style={{ padding: "2px 8px", marginLeft: k * 6, borderLeft: "2px solid var(--rule)" }}>
              <span className="mx-mono" style={{ fontSize: 11.5 }}>{a}</span>
              <span style={{ fontSize: 11, color: "var(--muted)" }}>
                {(MODEL[a] && MODEL[a].sl.length) ? ` · ${MODEL[a].sl.length} slots` : ""}
              </span>
            </div>
          ))}
          <div style={{ fontSize: 12, marginTop: 8 }}>
            {e.wp.length
              ? <>Trabalha nas fases <b className="mx-mono">{e.wp.join(", ")}</b> — {e.wp.map((p) => `${PHASES[p].m}() em ${e.po[String(p)]}`).join("; ")}.</>
              : e.d ? "Deriva de System (despacha por fase), mas nenhuma classe da cadeia implementa um método de fase."
                    : "Não deriva de System: não participa do despacho por fase."}
          </div>
          {e.ov && e.ov.length ? (
            <div style={{ fontSize: 11.5, color: "var(--muted)", marginTop: 6 }}>
              sobrescreve: <span className="mx-mono">{e.ov.join(", ")}</span>
            </div>
          ) : null}
          {derived.length ? (
            <div style={{ fontSize: 11.5, color: "var(--muted)", marginTop: 6 }}>
              derivadas ({derived.length}): <span className="mx-mono">{derived.slice(0, 12).join(", ")}{derived.length > 12 ? "…" : ""}</span>
            </div>
          ) : null}
        </div>
        <div>
          <div className="mx-lbl"><span>Slots ({slots.length})</span><span>{e.sl.length} próprios</span></div>
          <div>
            {slots.map(([s, from], k) => <div className="mx-slot" key={s + k}><span>{s}</span><span>{from}</span></div>)}
            {!slots.length && <div style={{ fontSize: 11.5, color: "var(--muted)" }}>EMPTY_SLOTTABLE em toda a cadeia.</div>}
          </div>
        </div>
      </div>

      <div style={{ marginTop: 12 }}>
        <div className="mx-lbl">
          <span>Código-fonte</span>
          <span>{ownSnippets.length ? `${ownSnippets.length} método${ownSnippets.length > 1 ? "s" : ""} extraído${ownSnippets.length > 1 ? "s" : ""}` : "nenhum método extraído"}</span>
        </div>
        {ownSnippets.length ? ownSnippets.map(([m, s]) => (
          <div key={m} style={{ marginBottom: 10 }}>
            <div className="mx-mono" style={{ fontSize: 11, color: "var(--muted)", marginBottom: 3 }}>{c}::{m} — {s.file}:{s.line}</div>
            <div className="mx-code">
              {s.lines.map((ln, k) => (
                <div key={k} className="mx-cl"><span className="mx-num">{s.line + k}</span><span className="mx-src">{ln || " "}</span></div>
              ))}
              {s.trunc && <div className="mx-codecut">⋯ corpo truncado nesta visualização ⋯</div>}
            </div>
          </div>
        )) : (
          <div style={{ fontSize: 11.5, color: "var(--muted)" }}>
            SNIPPETS é uma curadoria pequena (as trilhas de Execução), não as 342 classes inteiras — esta classe não está nela. arquivo:linha do topo do card continua valendo.
          </div>
        )}
      </div>
    </div>
  );
}
