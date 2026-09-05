import React, { useState, useEffect, useMemo, useRef, useCallback } from "react";

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
const SNIPPETS = {"Station::updateTC":{"file":"src/simulation/Station.cpp","line":258,"lines":["void Station::updateTC(const double dt)","{","   // Update the base::Timers","   if (isUpdateTimersEnabled()) {","      base::Timer::updateTimers(dt);","   }","","   // the I/O handler","   if (ioHandler != nullptr) {","      ioHandler->tcFrame(dt);","   }","","   // Process station inputs","   inputDevices(dt);","","   // Update the simulation","   if (sim != nullptr) sim->tcFrame(dt);","","   // Process station outputs","   outputDevices(dt);","","   // Our major subsystems","   if (sim != nullptr && igHosts != nullptr) {","      base::PairStream* playerList{sim->getPlayers()};","      base::List::Item* item{igHosts->getFirstItem()};","      while (item != nullptr) {","","         const auto pair = static_cast<base::Pair*>(item->getValue());","         const auto p = static_cast<AbstractIgHost*>(pair->object());","","         // Set ownship & player list","         p->setOwnship(ownship);","         p->setPlayerList(playerList);","","         // TC frame","         p->tcFrame(dt);","","         item = item->getNext();","      }","      if (playerList != nullptr) playerList->unref();","   }","","   // Startup RESET timer --","   //    Sends an initial RESET pulse after timeout","   //    (Some simulation may need this)","   if (startupResetTimer >= 0) {","      startupResetTimer -= dt;","      if (startupResetTimer < 0) {","         this->event(RESET_EVENT);","      }","   }","","   // Update the base class data","   BaseClass::updateTC(dt);","}"],"trunc":false},"Station::updateData":{"file":"src/simulation/Station.cpp","line":323,"lines":["void Station::updateData(const double dt)","{","   // Create a background thread (if needed)","   if (getBackgroundRate() > 0 && !doWeHaveTheBgThread()) {","      createBackgroundProcess();","   }","","   // Our simulation model and image generator host interfaces (if no separate thread)","   if (getBackgroundRate() == 0 && !doWeHaveTheBgThread()) {","      processBackgroundTasks(dt);","   }","","   // Create a network thread (if needed)","   if (getNetworkRate() > 0 && networks != nullptr && !doWeHaveTheNetThread()) {","      createNetworkProcess();","   }","","   // Our interoperability networks (if no separate thread)","   if (getNetworkRate() == 0 && networks != nullptr && !doWeHaveTheNetThread()) {","      processNetworkInputTasks(dt);","      processNetworkOutputTasks(dt);","   }","","   // ---","   // Background processing of the data recorders","   // ---","   if (dataRecorder != nullptr) dataRecorder->processRecords();","","   // Update base class data","   BaseClass::updateData(dt);","}"],"trunc":false},"Component::updateTC":{"file":"src/base/Component.cpp","line":243,"lines":["void Component::updateTC(const double dt)","{","    // Update all my children","    PairStream* subcomponents {getComponents()};","    if (subcomponents != nullptr) {","        if (selection != nullptr) {","            // When we've selected only one","            if (selected != nullptr) selected->tcFrame(dt);","        } else {","            // When we should update them all","            List::Item* item{subcomponents->getFirstItem()};","            while (item != nullptr) {","                const auto pair = static_cast<Pair*>(item->getValue());","                const auto obj = static_cast<Component*>( pair->object() );","                obj->tcFrame(dt);","                item = item->getNext();","            }","        }","        subcomponents->unref();","        subcomponents = nullptr;","    }","}"],"trunc":false},"Component::tcFrame":{"file":"src/base/Component.cpp","line":184,"lines":["void Component::tcFrame(const double dt)","{","   // ---","   // Collect start time","   // ---","   double tcStartTime {};","   if (isTimingStatsEnabled()) {","      #if defined(WIN32)","         LARGE_INTEGER fcnt;","         QueryPerformanceCounter(&fcnt);","         tcStartTime = static_cast<double>( fcnt.QuadPart );","      #else","         tcStartTime = getComputerTime();","      #endif","   }","","   // ---","   // Execute one time-critical frame","   // ---","   this->updateTC(dt);","","   // ---","   // Process timing data","   // ---","   if (isTimingStatsEnabled()) {","","      double dtime {};    // Delta time in MS","      #if defined(WIN32)","         LARGE_INTEGER cFreq;","         QueryPerformanceFrequency(&cFreq);","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Component::processComponents":{"file":"src/base/Component.cpp","line":583,"lines":["void Component::processComponents(","      PairStream* const list,","      const std::type_info& filter,","      Pair* const add,","      Component* const remove","   )","{","   PairStream* oldList {components.getRefPtr()};","","   // ---","   // Our dynamic_cast (see below) already filters on the Component class","   // ---","   bool skipFilter {};","   if (filter == typeid(Component)) {","      skipFilter = true;","   }","","   // ---","   // Create a new list, copy (filter) the component pairs and set their container pointers","   // ---","   const auto newList = new PairStream();","   if (list != nullptr) {","","      // Add the (filtered) components to the new list and set their container","      List::Item* item {list->getFirstItem()};","      while (item != nullptr) {","         const auto pair = static_cast<Pair*>(item->getValue());","         const auto cp = dynamic_cast<Component*>( pair->object() );","         if ( cp != nullptr && cp != remove && (skipFilter || cp->isClassType(filter)) ) {","            newList->put(pair);","            cp->container(this);","         } else if ( cp != nullptr && cp == remove ) {","            cp->container(nullptr);","         }","         item = item->getNext();","      }","","   }","","   // ---","   // Add the optional component","   // ---","   if (add != nullptr) {","      const auto cp = dynamic_cast<Component*>( add->object() );","      if ( cp != nullptr && (skipFilter || cp->isClassType(filter)) ) {","         newList->put(add);","         cp->container(this);","      }","   }","","   // ---","   // Swap lists","   // ---","   components = newList;","   newList->unref();","","   // ---","   // Anything selected?","   // ---","   if (selection != nullptr) {","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Player::updateSystemPointers":{"file":"src/models/player/Player.cpp","line":3141,"lines":["void Player::updateSystemPointers()","{","   // ---","   // Set base::Pair pointers for our primary systems located in our list of subcomponents","   // ---","   loadSysPtrs = false;","   setDynamicsModel( findByType(typeid(DynamicsModel)) );","   setDatalink( findByType(typeid(Datalink)) );","   setGimbal( findByType(typeid(Gimbal)) );","   setIrSystem( findByType(typeid(IrSystem)) );","   setNavigation( findByType(typeid(Navigation)) );","   setOnboardComputer( findByType(typeid(OnboardComputer)) );","   setPilot( findByType(typeid(Pilot)) );","   setRadio( findByType(typeid(Radio)) );","   setSensor( findByType(typeid(RfSensor)) );","   setStoresMgr( findByType(typeid(StoresMgr)) );","}"],"trunc":false},"System::updateTC":{"file":"src/models/system/System.cpp","line":84,"lines":["void System::updateTC(const double dt0)","{","   // We're nothing without an ownship ...","   if (ownship == nullptr && getOwnship() == nullptr) return;","","   // ---","   // Delta time","   // ---","","   // real or frozen?","   double dt{dt0};","   if (isFrozen()) dt = 0.0;","","   // Delta time for methods that are running every fourth phase","   double dt4{dt * 4.0};","","   // ---","   // Four phases per frame","   // ---","   WorldModel* sim{ownship->getWorldModel()};","   if (sim == nullptr) return;","","   switch (sim->phase()) {","","      case 0 : // Frame0 --- Dynamics method","         dynamics(dt4);","         break;","","      case 1 : // Frame1 --- Transmit method","         transmit(dt4);","         break;","","      case 2 : // Frame2 --- Receive method","         receive(dt4);","         break;","","      case 3 : // Frame3 --- Process method","         process(dt4);","         break;","   }","","   // ---","   // Last, update our base class","   // and use 'dt' because if we're frozen then so are our subcomponents.","   // ---","   BaseClass::updateTC(dt);","}"],"trunc":false},"Gimbal::dynamics":{"file":"src/models/system/Gimbal.cpp","line":221,"lines":["void Gimbal::dynamics(const double dt)","{","   servoController(dt);","   BaseClass::dynamics(dt);","}"],"trunc":false},"Gimbal::processPlayersOfInterest":{"file":"src/models/system/Gimbal.cpp","line":1318,"lines":["unsigned int Gimbal::processPlayersOfInterest(base::PairStream* const poi)","{","   const auto tdb0 = new Tdb(maxPlayers, this);","","   unsigned int ntgts{tdb0->processPlayers(poi)};","   setCurrentTdb(tdb0);","   tdb0->unref();","","   return ntgts;","}"],"trunc":false},"Antenna::process":{"file":"src/models/system/Antenna.cpp","line":131,"lines":["void Antenna::process(const double dt)","{","   BaseClass::process(dt);","","   // ---","   // Recycle emissions ...","   // Update emission queues: from 'in-use' to 'free'","   // ---","   if (recycle) {","      unsigned int n{inUseEmQueue.entries()};","","      for (unsigned int i = 0; i < n; i++) {","","         base::lock(inUseEmLock);","         Emission* em{inUseEmQueue.get()};","         base::unlock(inUseEmLock);","","         if (em != nullptr && em->getRefCount() > 1) {","            // Others are still referencing the emission, put back on in-use queue","            base::lock(inUseEmLock);","            inUseEmQueue.put(em);","            base::unlock(inUseEmLock);","         } else if (em != nullptr && em->getRefCount() <= 1) {","            // No one else is referencing the emission, push to the free stack","            em->clear();","            base::lock(freeEmLock);","            if (freeEmStack.isNotFull()) freeEmStack.push(em);","            else em->unref();","            base::unlock(freeEmLock);","         }","      }","   }","}"],"trunc":false},"Radar::transmit":{"file":"src/models/system/Radar.cpp","line":153,"lines":["void Radar::transmit(const double dt)","{","   BaseClass::transmit(dt);","","   // Transmitting, scanning and have an antenna?","   if ( !areEmissionsDisabled() && isTransmitting() ) {","      // Send the emission to the other player","      const auto em = new Emission();","      em->setFrequency(getFrequency());","      em->setBandwidth(getBandwidth());","      const double prf1{getPRF()};","      em->setPRF(prf1);","      int pulses{static_cast<int>(prf1 * dt + 0.5)};","      if (pulses == 0) pulses = 1; // at least one","      em->setPulses(pulses);","      const double p{getPeakPower()};","      em->setPower(p);","      em->setMaxRangeNM(getRange());","      em->setPulseWidth(getPulseWidth());","      em->setTransmitLoss(getRfTransmitLoss());","      em->setReturnRequest( isReceiverEnabled() );","      em->setTransmitter(this);","      getAntenna()->rfTransmit(em);","      em->unref();","   }","","}"],"trunc":false},"Autopilot::process":{"file":"src/models/system/Autopilot.cpp","line":187,"lines":["void Autopilot::process(const double dt)","{","   modeManager();","   headingController();","   altitudeController();","   velocityController();","","   BaseClass::process(dt);","}"],"trunc":false},"Navigation::process":{"file":"src/models/navigation/Navigation.cpp","line":191,"lines":["void Navigation::process(const double dt)","{","   BaseClass::process(dt);","","   // ---","   // Update our position, attitude and velocities","   // ---","   if (getOwnship() != nullptr) {","      velValid = updateSysVelocity();","      posValid = updateSysPosition();","      attValid = updateSysAttitude();","      magVarValid = updateMagVar();","   }","   else {","      posValid = false;","      attValid = false;","      velValid = false;","      magVarValid = false;","   }","","   // Update UTC","   double v {utc + dt};","   if (v >= base::time::D2S) {","      v = (v - base::time::D2S);","   }","   setUTC(v);","","   // ---","   // Update our primary route","   // ---","   if (priRoute != nullptr) priRoute->tcFrame(dt);","","   // Update our bullseye","   if (bull != nullptr) bull->compute(this);","","   // ---","   // Update our navigational steering data","   // ---","   updateNavSteering();","}"],"trunc":false},"Navigation::updateData":{"file":"src/models/navigation/Navigation.cpp","line":180,"lines":["void Navigation::updateData(const double dt)","{","   // ---","   // Update the BaseClass and our primary route","   // ---","   if (priRoute != nullptr) priRoute->updateData(dt);","}"],"trunc":false},"TrackManager::process":{"file":"src/models/system/trackmanager/TrackManager.cpp","line":155,"lines":["void TrackManager::process(const double dt)","{","   processTrackList(dt);","   BaseClass::process(dt);","}"],"trunc":false},"SimpleStoresMgr::process":{"file":"src/models/system/SimpleStoresMgr.cpp","line":52,"lines":["void SimpleStoresMgr::process(const double dt)","{","   BaseClass::process(dt);","","   // Weapon released timer","   if (wpnRelTimer > 0.0) {","      // decrease timer to zero","      wpnRelTimer -= dt;","   }","}"],"trunc":false},"Stores::releaseWeapon":{"file":"src/models/system/Stores.cpp","line":313,"lines":["AbstractWeapon* Stores::releaseWeapon(AbstractWeapon* const wpn)","{","   AbstractWeapon* flyout{};","","   Player* own{getOwnship()};","   if (wpn != nullptr && own != nullptr) {","","      // Release the weapon","      wpn->setLaunchVehicle(own);","      flyout = wpn->release();","","   }","","   return flyout;","}"],"trunc":false},"LaeroModel::dynamics":{"file":"src/models/dynamics/LaeroModel.cpp","line":90,"lines":["void LaeroModel::dynamics(const double dt)","{","    update4DofModel(dt);","    dT = dt;","}"],"trunc":false},"RacModel::dynamics":{"file":"src/models/dynamics/RacModel.cpp","line":70,"lines":["void RacModel::dynamics(const double dt)","{","    updateRAC(dt);","}"],"trunc":false},"OnboardComputer::process":{"file":"src/models/system/OnboardComputer.cpp","line":71,"lines":["void OnboardComputer::process(const double dt)","{","   BaseClass::process(dt);","}"],"trunc":false},"ScanGimbal::dynamics":{"file":"src/models/system/ScanGimbal.cpp","line":103,"lines":["void ScanGimbal::dynamics(const double dt)","{","   scanController(dt);","","   // Call BaseClass after to scan controller since the servo controller","   // is located in BaseClass.","   BaseClass::dynamics(dt);","}"],"trunc":false},"Datalink::dynamics":{"file":"src/models/system/Datalink.cpp","line":263,"lines":["void Datalink::dynamics(const double)","{","    //age queues","    mixr::base::Object* tempInQueue[MAX_MESSAGES]{};","    int numIn{};","    Message* msg{};","    while ((numIn < MAX_MESSAGES) && inQueue->isNotEmpty()) {","        mixr::base::Object* tempObj{inQueue->get()};","        msg = dynamic_cast<Message*>(tempObj);","        if (msg != nullptr) {","            if (base::getComputerTime() - msg->getTimeStamp() > msg->getLifeSpan()) {","                //remove message by not adding to list to be put back into queue","                msg->unref();","            } else {","                tempInQueue[numIn++] = msg;","            }","        } else if (tempObj != nullptr) {","            tempInQueue[numIn++] = tempObj;","        }","    }","    if (numIn != 0) {","        for(int i = 0; i < numIn; i++) {","            inQueue->put(tempInQueue[i]);","        }","    }","","    mixr::base::Object* tempOutQueue[MAX_MESSAGES]{};","    int numOut{};","    msg = nullptr;","    while((numOut < MAX_MESSAGES) && outQueue->isNotEmpty()) {","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Radio::receive":{"file":"src/models/system/Radio.cpp","line":212,"lines":["void Radio::receive(const double dt)","{","   BaseClass::receive(dt);","","   // Receiver losses","   const double noise{getRfRecvNoise()};","","   // ---","   // Process Emissions","   // ---","","   Emission* em = nullptr;","   double signal = 0;","","   // Get an emission from the queue","   base::lock(packetLock);","   if (np > 0) {","      np--; // Decrement 'np', now the array index","      em = packets[np];","      signal = signals[np];","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Radar::receive":{"file":"src/models/system/Radar.cpp","line":184,"lines":["void Radar::receive(const double dt)","{","   BaseClass::receive(dt);","","   // Can't do anything without an antenna","   if (getAntenna() == nullptr) return;","","   // Clear the next sweep","   csweep = computeSweepIndex( static_cast<double>(base::angle::R2DCC * getAntenna()->getAzimuth()) );","   clearSweep(csweep);","","   // Compute noise level","   // CGB moved here from RfSystem","   // Basically, we're simulation Hannen's S/I equation from page 356 of his notes.","   // Where I is N + J. J is noise from jamming.","   // Receiver Loss affects the total I, so we have to wait until this point to account for it.","   const double interference{(getRfRecvNoise() + jamSignal) * getRfReceiveLoss()};","   const double noise{getRfRecvNoise() * getRfReceiveLoss()};","   currentJamSignal = jamSignal * getRfReceiveLoss();","   int countNumJammedEm{};","","   // ---","   // Process Returned Emissions","   // ---","","   Emission* em{};","   double signal{};","","   // Get an emission from the queue","   base::lock(packetLock);","   if (np > 0) {","      np--; // Decrement 'np', now the array index","      em = packets[np];","      signal = signals[np];","   }","   base::unlock(packetLock);","","   while (em != nullptr) {","","      // exclude noise jammers (accounted for already in RfSystem::rfReceivedEmission)","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Radar::process":{"file":"src/models/system/Radar.cpp","line":325,"lines":["void Radar::process(const double dt)","{","   BaseClass::process(dt);","","   // Find the track manager","   TrackManager* tm{getTrackManager()};","   if (tm == nullptr) {","      // No track manager! Then just flush the input queue.","      base::lock(myLock);","      for (Emission* em = rptQueue.get(); em != nullptr; em = rptQueue.get()) {","         em->unref();","         rptSnQueue.get();","      }","      base::unlock(myLock);","   }","","   // ---","   // When end of scan, send all unsent reports to the track manager","   // ---","   if (endOfScanFlg) {","","      endOfScanFlg = false;","","      base::lock(myLock);","      for (unsigned int i = 0; i < numReports && i < MAX_REPORTS; i++) {","         if (tm != nullptr) {","            tm->newReport(reports[i], rptMaxSn[i]);","         }","         reports[i]->unref();","         reports[i] = nullptr;","         rptMaxSn[i] = 0;","      }","      numReports = 0;","      base::unlock(myLock);","   }","","","   // ---","   // Process our returned emissions into reports for the track manager","   //   1) Match each emission with existing reports","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Gun::process":{"file":"src/models/system/Guns.cpp","line":136,"lines":["void Gun::process(const double dt)","{","   BaseClass::process(dt);","","   // ---","   // Are we firing?","   // ---","   if (fire && (getRoundsRemaining() > 0 || isUnlimited()) ) {","      const double rps{computeBulletRatePerSecond()};","      const double bpi{rps * dt};","      rcount += bpi;","   }","","   // ---","   // Generate small burst of bullets at 10 hz","   // ---","   burstFrameTimer += dt;","   if (burstFrameTimer >= burstFrameTime) {","      burstFrameTimer = 0;","      if (rcount > 0) burstFrame();","   }","","   // ---","   // Burst timer","   // ---","   if (shortBurstTimer > 0 && fire) {","      shortBurstTimer -= dt;","      if (shortBurstTimer <= 0) {","         shortBurstTimer = 0;","         fire = false;","      }","   }","}"],"trunc":false},"AbstractWeapon::dynamics":{"file":"src/models/player/weapon/AbstractWeapon.cpp","line":253,"lines":["void AbstractWeapon::dynamics(const double dt)","{","   if (isMode(PRE_RELEASE)) {","      // Weapon is on the same side as the launcher","      setSide( getLaunchVehicle()->getSide() );","","      // Launch vehicles rotational matrix","      base::Matrixd lvM{getLaunchVehicle()->getRotMat()};","","      // Set weapon's position at launch","      // 1) Weapon's position is its position relative to the launcher (launcher's body coordinates)","      // 2) Rotate to earth coordinates","      // 3) Add the launcher's position","      const base::Vec2d ip{getInitPosition()};","      const base::Vec3d pos0b(ip.x(), ip.y(), -getInitAltitude());","      const base::Vec3d pos0e{pos0b * lvM}; // body to earth","      const base::Vec3d lpos{getLaunchVehicle()->getPosition()};","      const base::Vec3d pos1{lpos + pos0e};","      setPosition( pos1 );","","      // Weapon's orientation at launch","      const base::Vec3d ia{getInitAngles()};","      base::Matrixd rr;","      base::nav::computeRotationalMatrix( ia[0], ia[1], ia[2], &rr);","      rr *= lvM;","","      setRotMat(rr);","","      // Set velocities are the same as the launcher","      setVelocity( getLaunchVehicle()->getVelocity() );","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Player::dynamics":{"file":"src/models/player/Player.cpp","line":2764,"lines":["void Player::dynamics(const double dt)","{","   // ---","   // Local player ...","   // ---","   if (isLocalPlayer()) {","      // Update the external dynamics model (if any)","      if (getDynamicsModel() != nullptr) {","         // If we have a dynamics model ...","         getDynamicsModel()->freeze( isFrozen() );","         getDynamicsModel()->dynamics(dt);","      }","","      // Update our position","      positionUpdate(dt);","","      if (getNib() != nullptr || true) {","         if (!syncState1Ready) {","            syncState1.setGeocPosition(getGeocPosition());","            syncState1.setGeocVelocity(getGeocVelocity());","            syncState1.setGeocAcceleration(getGeocAcceleration());","            syncState1.setGeocEulerAngles(getGeocEulerAngles());","            syncState1.setAngularVelocities(getAngularVelocities());","            syncState1.setTimeExec(getWorldModel()->getExecTimeSec());","            syncState1.setTimeUtc(getWorldModel()->getSysTimeOfDay());","            syncState1.setValid(true);","            syncState1Ready = true;","            syncState2Ready = false;","            //std::cout << \"Set syncState1\" << std::endl;","         } else {","   // ... corpo truncado nesta visualizacao","}"],"trunc":true},"Simulation::phaseLoop":{"file":"src/simulation/Simulation.cpp","line":538,"lines":["   // ---","   BaseClass::updateTC(dt0);","","   // ---","   // Called once per frame -- Process 4 phases per frame","   // ---","   {","      // This locks the current player list for this time-critical frame","      base::safe_ptr<base::PairStream> currentPlayerList = players;","","      for (unsigned int f = 0; f < 4; f++) {","","         // Set the current phase","         setPhase(f);","","         if (reqTcThreads == 1) {","            // Our single TC thread","            updateTcPlayerList(currentPlayerList, (dt0/4.0), 1, 1);","         } else if (numTcThreads > 0) {","            // multiple threads","            for (unsigned short i = 0; i < numTcThreads; i++) {","","               // assign the threads from the pool","               unsigned int idx {static_cast<unsigned int>(i+1)};","               tcThreads[i]->start0(currentPlayerList, (dt0/4.0), idx, reqTcThreads);","            }","","            // we're the last thread","            updateTcPlayerList(currentPlayerList, (dt0/4.0), reqTcThreads, reqTcThreads);","","            // Now wait for the other thread(s) to complete","            base::SyncThread** pp {reinterpret_cast<base::SyncThread**>(&tcThreads[0])};","            base::SyncThread::waitForAllCompleted(pp, numTcThreads);","","         } else if (isMessageEnabled(MSG_ERROR)) {","            std::cerr << \"simulation::updateTC() ERROR, invalid T/C thread setup\";","            std::cerr << \"; reqTcThreads = \" << reqTcThreads;","            std::cerr << \"; numTcThreads = \" << numTcThreads;","            std::cerr << std::endl;","         }","      }","   }","","   // Update frame & cycle counts","   int cframe{static_cast<int>(frame() + 1)};","   if (cframe >= 16) {","      incCycle();","      cframe = 0;","   }","   setFrame(cframe);","   setPhase(0);","}","","//------------------------------------------------------------------------------","// Time critical thread processing for every n'th player starting"],"trunc":false},"Simulation::frameCount":{"file":"src/simulation/Simulation.cpp","line":580,"lines":["","   // Update frame & cycle counts","   int cframe{static_cast<int>(frame() + 1)};","   if (cframe >= 16) {","      incCycle();","      cframe = 0;","   }","   setFrame(cframe);","   setPhase(0);","}","","//------------------------------------------------------------------------------","// Time critical thread processing for every n'th player starting","// with the idx'th player","//------------------------------------------------------------------------------","void Simulation::updateTcPlayerList(","   base::PairStream* const playerList,"],"trunc":false},"Player::phaseSwitch":{"file":"src/models/player/Player.cpp","line":546,"lines":["               rfReflect[i]->unref();","               rfReflect[i] = nullptr;","            }","         }","      }","","      // ---","      // Delta time -- real or frozen?","      // ---","      double dt{dt0};","      if (isFrozen()) dt = 0.0;","","      // ---","      // Compute delta time for modules running every fourth phase","      // ---","      double dt4{dt * 4.0};     // Delta time for items running every fourth phase","      switch (getWorldModel()->phase()) {","","         // Phase 0 -- Dynamics","         case 0 : {","            // Our dynamics","            dynamics(dt4);","","            // Log our player's dynamic data just after its been updated ...","            if (dataLogTime > 0.0) {","               // When we have a data logging time, update the timer","               dataLogTimer -= dt4;","               if (dataLogTimer <= 0.0) {","                  // At timeout, log the player's data and ...","","                  BEGIN_RECORD_DATA_SAMPLE( getWorldModel()->getDataRecorder(), REID_PLAYER_DATA )","                     SAMPLE_1_OBJECT( this )","                  END_RECORD_DATA_SAMPLE()","","                  // reset the timer.","                  dataLogTimer = dataLogTime;","               }","            }","","            // Update signatures after we've updated our dynamics","            if (signature != nullptr) signature->updateTC(dt4);","            if (irSignature != nullptr) irSignature->updateTC(dt4);","         }","         break;","","         // Phase 1 -- Sensors transmit","         case 1 :","         break;","","         // Phase 2 -- Sensors Receive","         case 2 :","         break;","","         // Phase 3 -- PDL and other logic","         case 3 :","         break;","","      }","","      // ---","      // Notes:","      //  a) Remember that our subsystems in the components list (e.g., pilot, nav,","      //     sms and obc) are updated by our call to BaseClass:updateTC()","      //  b) We're calling BaseClass::updateTC() class because we want to update","      //     our player dynamics, etc before our subsystems.","      // ---","      BaseClass::updateTC(dt);","   }","}","","//------------------------------------------------------------------------------"],"trunc":false},"Simulation::updateTcPlayerList":{"file":"src/simulation/Simulation.cpp","line":595,"lines":["void Simulation::updateTcPlayerList(","   base::PairStream* const playerList,","   const double dt,","   const unsigned int idx,","   const unsigned int n)","{","   if (playerList != nullptr) {","      unsigned int index{idx};","      unsigned int count{};","      base::List::Item* item {playerList->getFirstItem()};","      while (item != nullptr) {","         count++;","         if (count == index) {","            base::Pair* pair {static_cast<base::Pair*>(item->getValue())};","            AbstractPlayer* ip {static_cast<AbstractPlayer*>(pair->object())};","            ip->tcFrame(dt);","            index += n;","         }","         item = item->getNext();","      }","   }","}",""],"trunc":false}};
const STATS = {"classes":342,"registered":224,"divergent":48,"withSlots":135,"slotsTotal":644,"phaseWork":81,"phaseOwn":37,"dispatch":44,"cpp":313};


const PHASES = [
  { n: 0, m: "dynamics", label: "Dinâmica" },
  { n: 1, m: "transmit", label: "Transmitem" },
  { n: 2, m: "receive", label: "Recebem" },
  { n: 3, m: "process", label: "Lógica" },
];
const DEPTH_LABELS = ["raiz", "executive e E/S", "players", "subsistemas", "componentes"];

/* ---------- consultas ao modelo ---------- */
const cls = (c) => MODEL[c] || null;
const chainOf = (c) => (MODEL[c] ? MODEL[c].ch : [c]);
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
        N("f16", "Aircraft", {
          edl: "ownship", via: "players:", player: true, thread: "tc",
          children: [
            N("dyn", "LaeroModel", { edl: "dyn", via: "components:", thread: "tc" }),
            N("sig", "SigSphere", { edl: "sig", via: "signature:", thread: "—",
              note: "Nenhum método de fase em toda a cadeia. É consultada pelo emissor via getRCS(), nunca atualizada pelo ciclo." }),
            N("radar", "Radar", {
              edl: "radar", via: "components:", thread: "tc+fundo",
              children: [
                N("ant", "Antenna", { edl: "ant", via: "components:", thread: "tc+fundo",
                  note: "Trabalha em DUAS fases: ScanGimbal::dynamics() move a varredura na fase 0 e Antenna::process() recicla emissões na fase 3. rfTransmit() é chamada pelo Radar dentro de transmit(), fora do despacho por fase." }),
              ],
            }),
            N("tm", "AirTrkMgr", { edl: "trkMgr", via: "components:", thread: "tc",
              note: "AirTrkMgr não implementa fase alguma: quem roda é TrackManager::process(), herdado." }),
            N("obc", "OnboardComputer", { edl: "obc", via: "components:", thread: "tc" }),
            N("nav", "Navigation", { edl: "nav", via: "components:", thread: "tc+fundo",
              note: "Implementa process() (fase 3) E updateData(). Ins e Gps derivam dela sem sobrescrever nada." }),
            N("route", "Route", { edl: "rota", via: "route:", thread: "fundo",
              note: "Deriva direto de Component, não de System: não há despacho por fase nenhum." }),
            N("ap", "Autopilot", { edl: "ap", via: "components:", thread: "tc",
              note: "O retorno dos comandos ao DynamicsModel é descartado. Um modelo sem heading hold não recua para o manche." }),
            N("sms", "SimpleStoresMgr", {
              edl: "sms", via: "components:", thread: "tc",
              note: "( StoresMgr ) no EDL constrói ESTA classe. A classe StoresMgr registra-se como BaseStoresMgr e não está na fábrica.",
              children: [
                N("msl1", "Missile", { edl: "1", via: "stores:", thread: "—" }),
                N("gun", "Gun", { edl: "5", via: "stores:", thread: "tc" }),
              ],
            }),
            N("dl", "Datalink", { edl: "dl", via: "components:", thread: "tc",
              note: "Implementa dynamics() — fase 0, não fase 3. A fila de mensagens é drenada junto com a física." }),
          ],
        }),
        N("mig", "Aircraft", {
          edl: "target01", via: "players:", player: true, thread: "tc",
          children: [
            N("dyn2", "RacModel", { edl: "dyn", via: "components:", thread: "tc" }),
            N("sig2", "SigConstant", { edl: "sig", via: "signature:", thread: "—" }),
          ],
        }),
        N("flyout", "Missile", { edl: "wpn_1", via: "addNewPlayer()", player: true, dynamic: true, thread: "tc" }),
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

/* ============ catálogo completo de modelos built-in (sem exceção) =========== */
/* O cenário acima é ilustrativo (uma composição possível). Esta segunda árvore   *
 * cobre as 122 classes do módulo mixr::models — cada uma que DECLARE_SUBCLASS,   *
 * registrada em fábrica ou não — agrupadas pela própria cadeia de herança, não   *
 * por um EDL específico. Geometria própria: um grid de mini-árvores por         *
 * categoria em vez de uma lista única indentada, para não empilhar 122 linhas   *
 * numa coluna só.                                                                */

/* GERADO por scripts/extract_execution_chain.py --catalog: os 122 declarados em mixr::models, agrupados por herança — SEM excecao (o agrupamento em categorias/"forests" abaixo é um passo manual sobre essa saída, ainda não versionado como script — ver docs/README.md). */
const CATALOG = [{"title":"Veículos — base","forests":[{"id":"c4_Player","cls":"Player","thread":"tc+fundo","children":[{"id":"c1_Building","cls":"Building","thread":"tc+fundo"},{"id":"c2_LifeForm","cls":"LifeForm","thread":"tc+fundo"},{"id":"c3_Ship","cls":"Ship","thread":"tc+fundo"}]}]},{"title":"Veículos aéreos","forests":[{"id":"c8_AirVehicle","cls":"AirVehicle","thread":"tc+fundo","children":[{"id":"c5_Aircraft","cls":"Aircraft","thread":"tc+fundo"},{"id":"c6_Helicopter","cls":"Helicopter","thread":"tc+fundo"},{"id":"c7_UnmannedAirVehicle","cls":"UnmannedAirVehicle","thread":"tc+fundo"}]}]},{"title":"Veículos terrestres","forests":[{"id":"c17_GroundVehicle","cls":"GroundVehicle","thread":"tc+fundo","children":[{"id":"c9_ArmoredVehicle","cls":"ArmoredVehicle","thread":"tc+fundo"},{"id":"c10_Artillery","cls":"Artillery","thread":"tc+fundo"},{"id":"c13_GroundStation","cls":"GroundStation","thread":"tc+fundo","children":[{"id":"c11_GroundStationRadar","cls":"GroundStationRadar","thread":"tc+fundo"},{"id":"c12_GroundStationUav","cls":"GroundStationUav","thread":"tc+fundo"}]},{"id":"c14_SamVehicle","cls":"SamVehicle","thread":"tc+fundo"},{"id":"c15_Tank","cls":"Tank","thread":"tc+fundo"},{"id":"c16_WheeledVehicle","cls":"WheeledVehicle","thread":"tc+fundo"}]}]},{"title":"Veículos espaciais","forests":[{"id":"c21_SpaceVehicle","cls":"SpaceVehicle","thread":"tc+fundo","children":[{"id":"c18_BoosterSpaceVehicle","cls":"BoosterSpaceVehicle","thread":"tc+fundo"},{"id":"c19_MannedSpaceVehicle","cls":"MannedSpaceVehicle","thread":"tc+fundo"},{"id":"c20_UnmannedSpaceVehicle","cls":"UnmannedSpaceVehicle","thread":"tc+fundo"}]}]},{"title":"Armamento (player)","forests":[{"id":"c32_AbstractWeapon","cls":"AbstractWeapon","thread":"tc+fundo","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto.","children":[{"id":"c22_Bomb","cls":"Bomb","thread":"tc+fundo"},{"id":"c23_Bullet","cls":"Bullet","thread":"tc+fundo"},{"id":"c27_Effect","cls":"Effect","thread":"tc+fundo","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto.","children":[{"id":"c24_Chaff","cls":"Chaff","thread":"tc+fundo"},{"id":"c25_Decoy","cls":"Decoy","thread":"tc+fundo"},{"id":"c26_Flare","cls":"Flare","thread":"tc+fundo"}]},{"id":"c31_Missile","cls":"Missile","thread":"tc+fundo","children":[{"id":"c28_Aam","cls":"Aam","thread":"tc+fundo","note":"Fábrica registra como \"AamMissile\" — nome de classe e nome de fábrica divergem."},{"id":"c29_Agm","cls":"Agm","thread":"tc+fundo","note":"Fábrica registra como \"AgmMissile\" — nome de classe e nome de fábrica divergem."},{"id":"c30_Sam","cls":"Sam","thread":"tc+fundo"}]}]}]},{"title":"Modelos de dinâmica","forests":[{"id":"c38_DynamicsModel","cls":"DynamicsModel","thread":"tc+fundo","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto.","children":[{"id":"c36_AerodynamicsModel","cls":"AerodynamicsModel","thread":"tc+fundo","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto.","children":[{"id":"c33_JSBSimModel","cls":"JSBSimModel","thread":"tc+fundo"},{"id":"c34_LaeroModel","cls":"LaeroModel","thread":"tc+fundo"},{"id":"c35_RacModel","cls":"RacModel","thread":"tc+fundo"}]},{"id":"c37_SpaceDynamicsModel","cls":"SpaceDynamicsModel","thread":"tc+fundo","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto."}]}]},{"title":"System — base","forests":[{"id":"c42_System","cls":"System","thread":"fundo","children":[{"id":"c39_CollisionDetect","cls":"CollisionDetect","thread":"tc+fundo"},{"id":"c40_Datalink","cls":"Datalink","thread":"tc+fundo"},{"id":"c41_OnboardComputer","cls":"OnboardComputer","thread":"tc+fundo"}]}]},{"title":"Sensores de RF","forests":[{"id":"c55_RfSystem","cls":"RfSystem","thread":"tc+fundo","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto.","children":[{"id":"c45_Radio","cls":"Radio","thread":"tc+fundo","children":[{"id":"c43_CommRadio","cls":"CommRadio","thread":"tc+fundo"},{"id":"c44_Iff","cls":"Iff","thread":"tc+fundo","note":"Deriva de Radio — nasce ANINHADO num Radio existente, nunca como irmão dele."}]},{"id":"c54_RfSensor","cls":"RfSensor","thread":"tc+fundo","children":[{"id":"c46_Jammer","cls":"Jammer","thread":"tc+fundo"},{"id":"c51_Radar","cls":"Radar","thread":"tc+fundo","children":[{"id":"c47_Gmti","cls":"Gmti","thread":"tc+fundo"},{"id":"c48_Sar","cls":"Sar","thread":"tc+fundo"},{"id":"c49_Stt","cls":"Stt","thread":"tc+fundo"},{"id":"c50_Tws","cls":"Tws","thread":"tc+fundo"}]},{"id":"c52_Rwr","cls":"Rwr","thread":"tc+fundo"},{"id":"c53_SensorMgr","cls":"SensorMgr","thread":"tc+fundo"}]}]}]},{"title":"Sensores IR","forests":[{"id":"c58_IrSystem","cls":"IrSystem","thread":"fundo","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto.","children":[{"id":"c57_IrSensor","cls":"IrSensor","thread":"tc+fundo","children":[{"id":"c56_MergingIrSensor","cls":"MergingIrSensor","thread":"tc+fundo","note":"reset() exige um AirAngleOnlyTrkMgrPT — beco sem saída: essa classe não é construível nesta fábrica."}]}]}]},{"title":"Assinaturas","forests":[{"id":"c66_RfSignature","cls":"RfSignature","thread":"fundo","reg":false,"note":"Fábrica registra como \"Signature\" — nome de classe e nome de fábrica divergem. Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto. Todo ( Signature ... ) do EDL cai numa das subclasses abaixo.","children":[{"id":"c59_SigAzEl","cls":"SigAzEl","thread":"fundo"},{"id":"c60_SigConstant","cls":"SigConstant","thread":"fundo"},{"id":"c63_SigPlate","cls":"SigPlate","thread":"fundo","children":[{"id":"c62_SigDihedralCR","cls":"SigDihedralCR","thread":"fundo","children":[{"id":"c61_SigTrihedralCR","cls":"SigTrihedralCR","thread":"fundo"}]}]},{"id":"c64_SigSphere","cls":"SigSphere","thread":"fundo"},{"id":"c65_SigSwitch","cls":"SigSwitch","thread":"fundo"}]},{"id":"c68_IrSignature","cls":"IrSignature","thread":"fundo","children":[{"id":"c67_AircraftIrSignature","cls":"AircraftIrSignature","thread":"fundo","note":"getAirframeSignature() derruba o processo se as 6 tabelas de assinatura não forem declaradas."}]},{"id":"c71_IrShape","cls":"IrShape","thread":"—","children":[{"id":"c69_IrBox","cls":"IrBox","thread":"—"},{"id":"c70_IrSphere","cls":"IrSphere","thread":"—"}]}]},{"title":"Navegação","forests":[{"id":"c74_Navigation","cls":"Navigation","thread":"tc+fundo","children":[{"id":"c72_Gps","cls":"Gps","thread":"tc+fundo","note":"Nenhum slot ou método próprio: só marca 'que tipo de Navigation' isto é."},{"id":"c73_Ins","cls":"Ins","thread":"tc+fundo","note":"Nenhum slot ou método próprio: só marca 'que tipo de Navigation' isto é."}]},{"id":"c75_Route","cls":"Route","thread":"fundo"},{"id":"c77_Steerpoint","cls":"Steerpoint","thread":"fundo","children":[{"id":"c76_Bullseye","cls":"Bullseye","thread":"fundo","note":"Subclasse de Steerpoint sem slot próprio — só um Steerpoint com papel fixo na rota."}]}]},{"title":"Armamento embarcado","forests":[{"id":"c84_ExternalStore","cls":"ExternalStore","thread":"fundo","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto.","children":[{"id":"c78_AvionicsPod","cls":"AvionicsPod","thread":"fundo"},{"id":"c79_FuelTank","cls":"FuelTank","thread":"fundo"},{"id":"c80_Gun","cls":"Gun","thread":"tc+fundo"},{"id":"c83_Stores","cls":"Stores","thread":"tc+fundo","children":[{"id":"c82_StoresMgr","cls":"StoresMgr","thread":"tc+fundo","reg":false,"note":"Fábrica registra como \"BaseStoresMgr\" — nome de classe e nome de fábrica divergem. Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto. ( StoresMgr ) no EDL nunca constrói esta classe — quem responde por esse nome é SimpleStoresMgr.","children":[{"id":"c81_SimpleStoresMgr","cls":"SimpleStoresMgr","thread":"tc+fundo","note":"Fábrica registra como \"StoresMgr\" — nome de classe e nome de fábrica divergem. ( StoresMgr ) no EDL constrói ESTA classe — a fábrica aponta o nome \"StoresMgr\" para cá."}]}]}]}]},{"title":"Giroscópio / gimbal","forests":[{"id":"c89_Gimbal","cls":"Gimbal","thread":"tc+fundo","children":[{"id":"c87_ScanGimbal","cls":"ScanGimbal","thread":"tc+fundo","children":[{"id":"c85_Antenna","cls":"Antenna","thread":"tc+fundo"},{"id":"c86_IrSeeker","cls":"IrSeeker","thread":"tc+fundo"}]},{"id":"c88_StabilizingGimbal","cls":"StabilizingGimbal","thread":"tc+fundo"}]}]},{"title":"Gerenciadores de pista","forests":[{"id":"c96_TrackManager","cls":"TrackManager","thread":"tc+fundo","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto.","children":[{"id":"c90_AirTrkMgr","cls":"AirTrkMgr","thread":"tc+fundo"},{"id":"c93_AngleOnlyTrackManager","cls":"AngleOnlyTrackManager","thread":"tc+fundo","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto.","children":[{"id":"c92_AirAngleOnlyTrkMgr","cls":"AirAngleOnlyTrkMgr","thread":"tc+fundo","children":[{"id":"c91_AirAngleOnlyTrkMgrPT","cls":"AirAngleOnlyTrkMgrPT","thread":"tc+fundo","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto. Referenciada por MergingIrSensor::reset(), mas sem branch em models/factory.cpp — não construível."}]}]},{"id":"c94_GmtiTrkMgr","cls":"GmtiTrkMgr","thread":"tc+fundo"},{"id":"c95_RwrTrkMgr","cls":"RwrTrkMgr","thread":"tc+fundo"}]}]},{"title":"Piloto","forests":[{"id":"c98_Pilot","cls":"Pilot","thread":"fundo","children":[{"id":"c97_Autopilot","cls":"Autopilot","thread":"tc+fundo"}]}]},{"title":"Ambiente","forests":[{"id":"c101_AbstractAtmosphere","cls":"AbstractAtmosphere","thread":"fundo","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto.","children":[{"id":"c100_IrAtmosphere","cls":"IrAtmosphere","thread":"fundo","children":[{"id":"c99_IrAtmosphere1","cls":"IrAtmosphere1","thread":"fundo"}]}]}]},{"title":"Ações da árvore (BT)","forests":[{"id":"c106_Action","cls":"Action","thread":"tc","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto.","children":[{"id":"c102_ActionCamouflageType","cls":"ActionCamouflageType","thread":"tc"},{"id":"c103_ActionDecoyRelease","cls":"ActionDecoyRelease","thread":"tc"},{"id":"c104_ActionImagingSar","cls":"ActionImagingSar","thread":"tc"},{"id":"c105_ActionWeaponRelease","cls":"ActionWeaponRelease","thread":"tc"}]}]},{"title":"Mensagens e pistas","forests":[{"id":"c109_SensorMsg","cls":"SensorMsg","thread":"—","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto.","children":[{"id":"c107_Emission","cls":"Emission","thread":"—","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto."},{"id":"c108_IrQueryMsg","cls":"IrQueryMsg","thread":"—","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto."}]},{"id":"c112_Track","cls":"Track","thread":"—","children":[{"id":"c110_IrTrack","cls":"IrTrack","thread":"—","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto."},{"id":"c111_RfTrack","cls":"RfTrack","thread":"—","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto."}]},{"id":"c114_Tdb","cls":"Tdb","thread":"—","reg":false,"note":"Fábrica registra como \"Gimbal_Tdb\" — nome de classe e nome de fábrica divergem. Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto.","children":[{"id":"c113_TdbIr","cls":"TdbIr","thread":"—","reg":false,"note":"Fábrica registra como \"Seeker_TdbIr\" — nome de classe e nome de fábrica divergem. Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto."}]},{"id":"c115_TargetData","cls":"TargetData","thread":"—"},{"id":"c116_Message","cls":"Message","thread":"—","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto."},{"id":"c117_Designator","cls":"Designator","thread":"—","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto."},{"id":"c118_Image","cls":"Image","thread":"—","reg":false,"note":"Fábrica registra como \"SarImage\" — nome de classe e nome de fábrica divergem. Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto."},{"id":"c119_SynchronizedState","cls":"SynchronizedState","thread":"—","reg":false,"note":"Não encontrada na cadeia name==X da fábrica — não instanciável via EDL direto."}]},{"title":"Mundo e agentes","forests":[{"id":"c120_WorldModel","cls":"WorldModel","thread":"fundo"},{"id":"c121_SimAgent","cls":"SimAgent","thread":"fundo"},{"id":"c122_MultiActorAgent","cls":"MultiActorAgent","thread":"fundo"}]}];

const CAT_NW = 128, CAT_NH = 24, CAT_ROW = 30, CAT_COL = 144, CAT_GAP = 30, CAT_PAD = 14, CAT_TITLE_H = 20;

function layoutCategory(cat) {
  let rowCursor = 0;
  const nodes = [];
  cat.forests.forEach((root) => {
    const place = (n, depth) => {
      const kids = n.children || [];
      if (!kids.length) { nodes.push({ ...n, lx: depth * CAT_COL, ly: rowCursor * CAT_ROW, depth }); rowCursor += 1; }
      else {
        kids.forEach((k) => place(k, depth + 1));
        const f = nodes.find((m) => m.id === kids[0].id), l = nodes.find((m) => m.id === kids[kids.length - 1].id);
        nodes.push({ ...n, lx: depth * CAT_COL, ly: (f.ly + l.ly) / 2, depth });
      }
    };
    place(root, 0);
    rowCursor += 1; // linha em branco entre florestas da mesma categoria
  });
  const maxDepth = nodes.reduce((m, n) => Math.max(m, n.depth), 0);
  const width = maxDepth * CAT_COL + CAT_NW + CAT_PAD * 2;
  const height = (rowCursor - 1) * CAT_ROW + CAT_NH + CAT_TITLE_H + CAT_PAD * 2;
  return { nodes, width, height };
}

function packCategories(categories, targetWidth) {
  let x = 0, y = 0, shelfH = 0;
  const placed = categories.map((cat) => {
    const laid = layoutCategory(cat);
    if (x > 0 && x + laid.width > targetWidth) { x = 0; y += shelfH + CAT_GAP; shelfH = 0; }
    const ox = x, oy = y;
    const nodes = laid.nodes.map((n) => ({ ...n, x: n.lx + ox + CAT_PAD, y: n.ly + oy + CAT_PAD + CAT_TITLE_H + CAT_NH / 2 }));
    x += laid.width + CAT_GAP;
    shelfH = Math.max(shelfH, laid.height);
    return { title: cat.title, ox, oy, width: laid.width, height: laid.height, nodes };
  });
  const totalW = Math.max(CAT_GAP, ...placed.map((c) => c.ox + c.width)) + CAT_GAP;
  const totalH = y + shelfH + CAT_GAP;
  return { placed, totalW, totalH };
}

const CATALOG_PACKED = packCategories(CATALOG, 1360);
const CATALOG_ALL = CATALOG_PACKED.placed.flatMap((cat) => cat.nodes);
const CATALOG_EDGES = [];
CATALOG_ALL.forEach((n) => (n.children || []).forEach((c) => CATALOG_EDGES.push([n.id, c.id])));
CATALOG_ALL.forEach((n) => { byId[n.id] = n; });

const NAME_LINKS = [
  { from: "radar", to: "ant", slot: "antennaName:" },
  { from: "radar", to: "tm", slot: "trackManagerName:" },
  { from: "ap", to: "mig", slot: "leadPlayerName:" },
];

/* =============================== EDL ================================ */

const EDL_TEXT = `( Station
   tcRate: 50            // thread TC a 50 Hz
   bgRate: 20            // thread de fundo -- taxa PROPRIA
   netRate: 50
   ownship: "ownship"
   startupResetTimer: ( Seconds 0.1 )   // sem isto nada roda

   ioHandler:    ( IoHandler )     // NAO registrada na fabrica
   dataRecorder: ( DataRecorder )
   networks:   { net1: ( DisNetIO ) }

   simulation: ( WorldModel
      terrain: ( QuadMap )

      players: {

         ownship: ( Aircraft
            side: blue   type: "F-16"   id: 1
            signature: ( SigSphere radius: 4.0 )
            initPosition: [ 0 0 -10000 ]
            components: {

               dyn: ( LaeroModel )

               radar: ( Radar
                  antennaName: ant          // resolvido por STRING
                  trackManagerName: trkMgr  // idem
                  frequency: ( GigaHertz 9.5 )
                  components: {
                     ant: ( Antenna
                        polarization: horizontal
                        gain: ( dB 42 )
                        searchVolume: [ 1.0472 0.05 ]
                        numBars: 2
                        maxPlayersOfInterest: 20
                     )
                  }
               )

               trkMgr: ( AirTrkMgr
                  maxTracks: 20   maxTrackAge: ( Seconds 3.0 )
                  alpha: 1.0   beta: 0.0
               )

               obc: ( OnboardComputer )

               nav: ( Navigation
                  route: ( Route
                     to: 1
                     components: { ( Steerpoint latitude: ... ) }
                  )
               )

               ap: ( Autopilot
                  navMode: true
                  maxTurnRate: ( Degrees 3.0 )
               )

               sms: ( StoresMgr           // constroi SimpleStoresMgr!
                  stores: {
                     1: ( Missile type: "AIM-120C"
                          maxTOF: ( Seconds 60.0 ) tsg: ( Seconds 1.5 ) )
                     5: ( Gun rounds: 510 rate: 6000 )
                  }
               )

               dl: ( Datalink )
            }
         )

         target01: ( Aircraft
            side: red   type: "MiG-29"   id: 1000
            signature: ( SigConstant rcs: ( SquareMeters 12.0 ) )
            components: { dyn: ( RacModel ) }
         )
      }
   )
)`.split("\n");

const EDL_RANGE = {
  station: [0, 9], io: [7, 7], rec: [8, 8], net: [9, 9],
  sim: [11, 88], terr: [12, 12], f16: [16, 78], dyn: [21, 21],
  radar: [23, 36], ant: [27, 34], tm: [38, 41], obc: [43, 43],
  nav: [45, 50], route: [46, 49], ap: [52, 55], sms: [57, 63],
  msl1: [59, 60], gun: [61, 61], dl: [65, 65],
  mig: [69, 73], dyn2: [72, 72], sig2: [71, 71], sig: [18, 18], flyout: [57, 63],
};

/* ============================ geradores ============================= */

const DT = 0.02, FRAMES = 3, LAUNCH_FRAME = 1;
const fmt = (v) => `${(v * 1000).toFixed(1)} ms`;

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

  if (node.id === "f16" && ph.n === 0 && fr === 0)
    push({ kind: "note", node: "f16", counters: ctr, dt, src: "Player::updateSystemPointers", hl: [2, 14],
      title: "loadSysPtrs — a varredura por tipo",
      body: "Os ponteiros de sistema são resolvidos por findByType(). É por isso que a ordem dos filhos no EDL é irrelevante, e por que um Player sem DynamicsModel não é erro: cada getter testa o ponteiro antes de usar." });

  if (node.id === "ant" && ph.n === 1)
    push({ kind: "rf", node: "ant", to: "mig", counters: ctr, dt, src: "Radar::transmit", hl: [10, 24],
      title: "Radar::transmit() → Antenna::rfTransmit() → alvo->event(RF_EMISSION)",
      body: "Note que quem transmite é o Radar, não a antena: Antenna não implementa transmit(). O Radar monta a Emission e chama rfTransmit() da antena nomeada em antennaName. A emissão chega ao alvo como evento.",
      warn: "Aresta invisível a qualquer análise estática: nenhum call graph liga Antenna a Aircraft." });

  if (node.id === "tm" && ph.n === 3)
    push({ kind: "name", node: "tm", from: "radar", counters: ctr, dt, src: "TrackManager::process", hl: [0, 4],
      title: "TrackManager::process() — herdado por AirTrkMgr",
      body: "AirTrkMgr não sobrescreve process(). O ponteiro do gerente veio do slot trackManagerName, resolvido por string dentro do Radar.",
      warn: "Erro de digitação em trackManagerName não produz erro de carga: o sistema simplesmente não faz nada." });

  if (node.id === "sms" && ph.n === 3 && fr === LAUNCH_FRAME) {
    push({ kind: "release", node: "sms", to: "flyout", counters: ctr, dt, src: "Stores::releaseWeapon", hl: [0, 14],
      title: "Stores::releaseWeapon() — o míssil entra na simulação",
      body: "wpn->release() muda o modo para ACTIVE e chama addNewPlayer(). Do próximo quadro em diante o míssil é percorrido nas quatro fases como qualquer outro player.",
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
    stack: [{ label: "Station::updateData", node: "station" }, { label: "Player::updateData", node: "f16" }, { label: "Navigation::updateData", node: "nav" }],
    title: "Navigation::updateData() — e também Navigation::process()",
    body: "A navegação é dos poucos subsistemas que trabalham nos dois caminhos: updateData() no fundo e process() na fase 3. Os dados de pilotagem que o Autopilot lê podem, portanto, ser de outro quadro." });
  p({ kind: "bg", node: "ant", src: "Gimbal::processPlayersOfInterest", hl: [0, 9],
    stack: [{ label: "Station::updateData", node: "station" }, { label: "RfSystem::updateData", node: "radar" }, { label: "Gimbal::processPlayersOfInterest", node: "ant" }],
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
  p({ kind: "reset", node: "f16", src: "Component::processComponents", hl: [20, 40],
    stack: [{ label: "Station::reset", node: "station" }, { label: "Simulation::reset", node: "sim" }, { label: "Player::reset", node: "f16" }],
    title: "A porta de tipo de processComponents()",
    body: "Um filho que não seja Component é descartado em silêncio, e nem o parser nem isValid() acusam. O sintoma aparece depois, como um subsistema que não faz nada." });
  return st;
}

const TRACES = { tc: { label: "Quadro", build: traceFrames }, bg: { label: "Thread de fundo", build: traceBackground }, reset: { label: "Reset", build: traceReset } };

/* ============================== layout ============================== */

const NW = 142, NH = 34, ROW = 42, COL = 180;

function layout(root) {
  const nodes = []; let y = 0;
  (function place(n, depth) {
    const kids = n.children || [];
    if (!kids.length) { nodes.push({ ...n, x: depth * COL, y: y * ROW, depth }); y += 1; }
    else {
      kids.forEach((k) => place(k, depth + 1));
      const f = nodes.find((m) => m.id === kids[0].id);
      const l = nodes.find((m) => m.id === kids[kids.length - 1].id);
      nodes.push({ ...n, x: depth * COL, y: (f.y + l.y) / 2, depth });
    }
  })(root, 0);
  return nodes;
}

const THREAD_COLOR = { tc: "#16232E", fundo: "#3D6C8C", rede: "#4A6B4F", "tc+fundo": "#3D6C8C", "—": "#C6CDC3" };

/* =============================== CSS ================================ */

const CSS = `
.mx { --paper:#E6E9E3; --panel:#DCE0D9; --ink:#16232E; --muted:#6E7A76;
  --rule:#C6CDC3; --hot:#B4661E; --rf:#8C2F3D; --bgc:#3D6C8C; --ok:#4A6B4F;
  --new:#7A5B9B; --code:#1B2730; --codeink:#CFD8CE;
  --mono: ui-monospace,'JetBrains Mono','SF Mono',Menlo,monospace;
  --sans: 'Inter',system-ui,-apple-system,sans-serif;
  background:var(--paper); color:var(--ink); font-family:var(--sans);
  font-size:13.5px; line-height:1.5; }
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
  background:#EAEDE7; margin-bottom:12px; overflow:hidden; }
.mx-svgwrap { height:clamp(300px, 46vh, 780px); width:100%; }
.mx-svgwrap svg { width:100%; height:100%; display:block; touch-action:none; cursor:grab; }
.mx-svgwrap svg:active { cursor:grabbing; }
.mx-zoom { position:absolute; top:8px; right:8px; display:flex; gap:3px; z-index:2; }
.mx-zbtn { font:inherit; font-size:12px; width:26px; height:26px; padding:0; cursor:pointer;
  border:1px solid var(--rule); background:var(--paper); color:var(--ink); border-radius:2px; }
.mx-zbtn:hover { border-color:var(--ink); }
.mx-zbtn[data-w="1"] { width:auto; padding:0 8px; }
.mx-panes { display:grid; gap:12px; grid-template-columns:repeat(auto-fit, minmax(330px, 1fr)); align-items:start; }
.mx-pane { min-width:0; display:flex; flex-direction:column; }
.mx-card { background:var(--panel); padding:11px 13px; border-radius:2px; }
.mx-lbl { font-size:11.5px; color:var(--muted); margin-bottom:5px; display:flex;
  justify-content:space-between; gap:8px; align-items:baseline; }
.mx-mono { font-family:var(--mono); }
.mx-code { background:var(--code); color:var(--codeink); font-family:var(--mono);
  font-size:11.5px; line-height:17px; padding:9px 0; height:clamp(220px, 32vh, 440px);
  overflow:auto; border-radius:2px; }
.mx-edl { background:#F0F2EC; border:1px solid var(--rule); font-family:var(--mono);
  font-size:11.5px; line-height:17px; padding:9px 0; height:clamp(220px, 32vh, 440px);
  overflow:auto; border-radius:2px; }
.mx-cl { display:flex; }
.mx-cl[data-on="1"] { background:#2E4250; }
.mx-edl .mx-cl[data-on="1"] { background:#E2DBCE; }
.mx-num { width:42px; text-align:right; padding-right:9px; color:#5E7280; flex-shrink:0; }
.mx-edl .mx-num { width:30px; color:#A3ADA4; }
.mx-src { white-space:pre; border-left:2px solid transparent; padding-left:8px; }
.mx-cl[data-on="1"] .mx-src { border-left-color:var(--hot); }
.mx-node { cursor:pointer; }
.mx-node rect, .mx-node text { transition:fill 130ms, stroke 130ms, opacity 130ms; }
.mx-btn { font:inherit; font-size:12.5px; padding:5px 12px; border:1px solid var(--ink);
  background:transparent; color:var(--ink); border-radius:2px; cursor:pointer; }
.mx-btn:hover { background:var(--rule); }
.mx-btn[data-primary="1"] { background:var(--ink); color:var(--paper); }
.mx-transport { position:fixed; left:0; right:0; bottom:0; background:var(--paper);
  border-top:1px solid var(--rule); padding:8px 18px; display:flex; align-items:center;
  gap:8px; flex-wrap:wrap; z-index:6; }
.mx-tl { display:flex; height:24px; gap:1px; flex:1; min-width:170px; cursor:pointer; align-items:flex-end; }
.mx-seg { flex:1; min-width:1px; border-radius:1px 1px 0 0; }
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
  border-top:1px solid var(--rule); padding:6px 10px; background:var(--paper); }
.mx-stats { display:flex; gap:16px; flex-wrap:wrap; font-size:12px; color:var(--muted);
  margin-bottom:10px; }
.mx-stats b { color:var(--ink); font-family:var(--mono); font-weight:600; }
.mx-slot { display:flex; gap:8px; font-family:var(--mono); font-size:11px; padding:1px 0; }
.mx-slot span:first-child { min-width:132px; color:var(--ink); }
.mx-slot span:last-child { color:var(--muted); }
@media (prefers-reduced-motion:reduce) { .mx * { transition:none !important; } }
`;

/* =============================== app ================================ */

export default function App() {
  const [mode, setMode] = useState("exec");
  const [focus, setFocus] = useState(null);
  return (
    <div className="mx">
      <style>{CSS}</style>
      <div className="mx-bar">
        <div>
          <h1 className="mx-h1">MIXR — execução, EDL e {STATS.classes} classes built-in</h1>
          <p className="mx-sub">Extraído da árvore de fontes: {STATS.cpp} arquivos .cpp, {STATS.registered} classes registradas, {STATS.slotsTotal} slots</p>
        </div>
        <div className="mx-tabs">
          <button className="mx-tab" data-on={mode === "exec" ? 1 : 0} onClick={() => setMode("exec")}>Execução</button>
          <button className="mx-tab" data-on={mode === "cat" ? 1 : 0} onClick={() => setMode("cat")}>Catálogo</button>
        </div>
      </div>
      {mode === "exec"
        ? <Exec focus={focus} setFocus={setFocus} />
        : <Catalog onOpen={(c) => { setFocus(c); setMode("exec"); }} />}
    </div>
  );
}

/* ---------------------------- execução ----------------------------- */

function Exec({ focus, setFocus }) {
  const [traceKey, setTraceKey] = useState("tc");
  const [showIdle, setShowIdle] = useState(false);
  const [showNames, setShowNames] = useState(false);
  const [i, setI] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState(650);
  const [pinned, setPinned] = useState(null);
  const [view, setView] = useState({ k: 1, x: 0, y: 0 });
  const [execView, setExecView] = useState("trace");
  const drag = useRef(null);

  useEffect(() => {
    if (!focus) return;
    const n = ALL.find((x) => x.cls === focus);
    if (n) setPinned(n.id);
    setFocus(null);
  }, [focus, setFocus]);

  const raw = useMemo(() => TRACES[traceKey].build(), [traceKey]);
  const trace = useMemo(() => (showIdle ? raw : raw.filter((s) => !s.idle)), [raw, showIdle]);
  const nodes = useMemo(() => layout(SCENARIO), []);
  const pos = useMemo(() => Object.fromEntries(nodes.map((n) => [n.id, n])), [nodes]);

  const idx = Math.min(i, trace.length - 1);
  const step = trace[idx] || {};

  useEffect(() => setI(0), [traceKey, showIdle]);
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

  const detail = pinned ? byId[pinned] : (execView === "catalog" ? CATALOG_ALL[0] : byId[step.node] || byId.station);
  const dm = cls(detail.cls) || {};
  const pathEdges = useMemo(() => new Set(ancestors(step.node || "station").map(([a, b]) => a + ">" + b)), [step.node]);

  const snip = SNIPPETS[step.src];
  const codeRef = useRef(null);
  const edlRef = useRef(null);
  const edlRange = EDL_RANGE[detail.id] || EDL_RANGE.station;
  useEffect(() => { if (codeRef.current && step.hl) codeRef.current.scrollTop = Math.max(0, (step.hl[0] - 3) * 17); }, [step]);
  useEffect(() => { if (edlRef.current) edlRef.current.scrollTop = Math.max(0, (edlRange[0] - 3) * 17); }, [detail.id]);

  const edges = [];
  ALL.forEach((n) => (n.children || []).forEach((c) => edges.push([n.id, c.id])));
  const W = 4 * COL + NW + 30;
  const H = nodes.reduce((m, n) => Math.max(m, n.y), 0) + NH + 30;
  const ctr = step.counters || {};
  const curPhase = ctr.phase;

  const bandFor = (rootId) => {
    const ids = flat(byId[rootId]).map((n) => n.id);
    const ys = ids.map((id) => pos[id].y), xs = ids.map((id) => pos[id].x);
    return { y0: Math.min(...ys) - NH / 2 - 5, y1: Math.max(...ys) + NH / 2 + 5, x0: Math.min(...xs) - 6, x1: Math.max(...xs) + NW + 6 };
  };

  const segColor = (s) =>
    s.kind === "rf" ? "var(--rf)" : s.kind === "release" ? "var(--new)" :
    s.kind === "name" ? "var(--ok)" : s.kind === "phase" ? "var(--ink)" :
    s.counters && s.counters.phase != null ? ["#9AA79F", "#8FA0A8", "#A8A08F", "#9E93A8"][s.counters.phase] : "var(--rule)";

  const onWheel = (e) => { e.preventDefault(); const f = e.deltaY < 0 ? 1.12 : 1 / 1.12; setView((v) => ({ ...v, k: Math.max(0.4, Math.min(4, v.k * f)) })); };
  const onDown = (e) => { drag.current = { x: e.clientX, y: e.clientY, vx: view.x, vy: view.y }; e.currentTarget.setPointerCapture(e.pointerId); };
  const onMove = (e) => { if (!drag.current) return; setView((v) => ({ ...v, x: drag.current.vx + (e.clientX - drag.current.x), y: drag.current.vy + (e.clientY - drag.current.y) })); };
  const onUp = () => { drag.current = null; };

  const slots = allSlots(detail.cls);

  return (
    <>
      <div className="mx-body">
        <div className="mx-tabs" style={{ marginBottom: 10 }}>
          <button className="mx-tab" data-on={execView === "trace" ? 1 : 0} onClick={() => setExecView("trace")}>Cenário ilustrativo</button>
          <button className="mx-tab" data-on={execView === "catalog" ? 1 : 0} onClick={() => setExecView("catalog")}>Catálogo completo — {CATALOG_ALL.length} classes, sem exceção</button>
        </div>

        {execView === "trace" && (
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
                  <div key={p.n} style={{ padding: "3px 9px", borderRadius: 2, fontSize: 11.5, background: on ? "var(--ink)" : "var(--panel)", color: on ? "var(--paper)" : "var(--muted)" }}>
                    <span className="mx-mono">{p.n}</span> {p.label}
                  </div>
                );
              })}
            </div>
          )}
        </div>
        )}

        {execView === "catalog" && (
          <p style={{ fontSize: 12.5, color: "var(--muted)", margin: "0 0 10px", maxWidth: 900 }}>
            As {CATALOG_ALL.length} classes que <code className="mx-mono">DECLARE_SUBCLASS</code> em <code className="mx-mono">mixr::models</code>, agrupadas pela própria cadeia de herança em {CATALOG_PACKED.placed.length} categorias — não é o cenário acima, é o módulo inteiro. Clique num nó para ver herança e slots abaixo.
          </p>
        )}

        {execView === "trace" && (
        <div className="mx-graph">
          <div className="mx-zoom">
            <button className="mx-zbtn" onClick={() => setView((v) => ({ ...v, k: Math.min(4, v.k * 1.2) }))} aria-label="Aproximar">+</button>
            <button className="mx-zbtn" onClick={() => setView((v) => ({ ...v, k: Math.max(0.4, v.k / 1.2) }))} aria-label="Afastar">−</button>
            <button className="mx-zbtn" data-w="1" onClick={() => setView({ k: 1, x: 0, y: 0 })}>ajustar</button>
          </div>
          <div className="mx-svgwrap">
            <svg viewBox={`-14 -30 ${W} ${H + 30}`} preserveAspectRatio="xMidYMid meet"
                 onWheel={onWheel} onPointerDown={onDown} onPointerMove={onMove} onPointerUp={onUp} onPointerLeave={onUp}>
              <g transform={`translate(${view.x},${view.y}) scale(${view.k})`} style={{ transformOrigin: "center" }}>
                {DEPTH_LABELS.map((l, d) => (
                  <g key={d}>
                    <text x={d * COL} y={-14} className="mx-mono" style={{ fontSize: 10, fill: "var(--muted)" }}>{l}</text>
                    {d > 0 && <line x1={d * COL - 20} y1={-24} x2={d * COL - 20} y2={H - 26} stroke="var(--rule)" strokeWidth="1" strokeDasharray="2 4" />}
                  </g>
                ))}
                {["f16", "mig"].map((pid) => {
                  const b = bandFor(pid);
                  return <rect key={pid} x={b.x0} y={b.y0} width={b.x1 - b.x0} height={b.y1 - b.y0} rx="3" fill="#E0E4DC" />;
                })}
                {edges.map(([a, b]) => {
                  const p = pos[a], q = pos[b], child = byId[b];
                  const hidden = child.dynamic && (!launched || vanished);
                  const onPath = pathEdges.has(a + ">" + b);
                  const mid = p.x + NW + 16;
                  return (
                    <g key={a + b} opacity={hidden ? 0.22 : 1}>
                      <path d={`M ${p.x + NW} ${p.y} H ${mid} V ${q.y} H ${q.x}`} fill="none" stroke={onPath ? "var(--hot)" : "var(--rule)"} strokeWidth={onPath ? 2 : 1} />
                      {child.via && <text x={mid + 4} y={q.y - 4} className="mx-mono" style={{ fontSize: 8.5, fill: onPath ? "var(--hot)" : "#9AA59D" }}>{child.via}</text>}
                      {onPath && step.dt != null && <text x={mid + 4} y={q.y + 10} className="mx-mono" style={{ fontSize: 8.5, fill: "var(--hot)" }}>dt {fmt(step.dt)}</text>}
                    </g>
                  );
                })}
                {showNames && NAME_LINKS.map((l) => {
                  const a = pos[l.from], b = pos[l.to];
                  return (
                    <g key={l.slot} opacity="0.8">
                      <path d={`M ${a.x + NW / 2} ${a.y + NH / 2} C ${a.x - 26} ${a.y + 30}, ${b.x - 26} ${b.y - 30}, ${b.x + NW / 2} ${b.y - NH / 2}`} fill="none" stroke="var(--ok)" strokeWidth="1.3" strokeDasharray="2 3" />
                      <text x={Math.min(a.x, b.x) - 28} y={(a.y + b.y) / 2} textAnchor="end" className="mx-mono" style={{ fontSize: 8.5, fill: "var(--ok)" }}>{l.slot}</text>
                    </g>
                  );
                })}
                {step.kind === "rf" && (
                  <g>
                    <path d={`M ${pos.ant.x + NW / 2} ${pos.ant.y - NH / 2} C ${pos.ant.x} ${pos.ant.y - 80}, ${pos.mig.x + NW} ${pos.mig.y - 80}, ${pos.mig.x + NW / 2} ${pos.mig.y - NH / 2}`} fill="none" stroke="var(--rf)" strokeWidth="1.8" strokeDasharray="5 3" />
                    <text x={(pos.ant.x + pos.mig.x) / 2 + NW / 2} y={pos.ant.y - 66} textAnchor="middle" className="mx-mono" style={{ fontSize: 10, fill: "var(--rf)" }}>event(RF_EMISSION)</text>
                  </g>
                )}
                {step.kind === "release" && (
                  <path d={`M ${pos.sms.x + NW / 2} ${pos.sms.y + NH / 2} C ${pos.sms.x} ${pos.sms.y + 70}, ${pos.flyout.x + 30} ${pos.flyout.y - 50}, ${pos.flyout.x + NW / 2} ${pos.flyout.y - NH / 2}`} fill="none" stroke="var(--new)" strokeWidth="2" strokeDasharray="4 3" />
                )}
                {nodes.map((n) => {
                  const active = step.node === n.id;
                  const inStack = (step.stack || []).some((s) => s.node === n.id);
                  const running = active && step.runs;
                  const ghost = n.dynamic && (!launched || vanished);
                  const never = !n.phases.length;
                  const v = visits[n.id] || 0;
                  return (
                    <g key={n.id} className="mx-node" transform={`translate(${n.x},${n.y - NH / 2})`} onClick={() => setPinned(pinned === n.id ? null : n.id)}>
                      <rect x="0" y="0" width={NW} height={NH} rx="2"
                        fill={running ? "var(--hot)" : active ? "#F0EAE2" : never ? "#E2E5DF" : "var(--paper)"}
                        stroke={pinned === n.id ? "var(--ink)" : running ? "var(--hot)" : active ? "var(--ink)" : inStack ? "var(--muted)" : "var(--rule)"}
                        strokeWidth={active || pinned === n.id ? 1.6 : 1}
                        strokeDasharray={ghost ? "3 2" : "0"} opacity={ghost ? 0.45 : 1} />
                      <rect x="0" y="0" width="3" height={NH} fill={THREAD_COLOR[n.thread] || "var(--rule)"} opacity={ghost ? 0.4 : 0.9} />
                      <text x="9" y="14" className="mx-mono" style={{ fontSize: 11, fontWeight: 600, fill: running ? "var(--paper)" : never ? "var(--muted)" : "var(--ink)", opacity: ghost ? 0.55 : 1 }}>{n.cls}</text>
                      <text x="9" y="26" className="mx-mono" style={{ fontSize: 9, fill: running ? "#F0E2D4" : "#8F9A93", opacity: ghost ? 0.5 : 1 }}>
                        {n.edl}{n.player ? " ·player" : ""}{n.disp ? "" : " ·s/System"}
                      </text>
                      <g transform={`translate(${NW - 48}, 21)`} opacity={ghost ? 0.5 : 1}>
                        {PHASES.map((p) => {
                          const has = n.phases.includes(p.n);
                          const own = has && phaseOwner(n.cls, p.n) === n.cls;
                          const now = has && curPhase === p.n;
                          return <rect key={p.n} x={p.n * 10} y="0" width="7" height="7" rx="1"
                            fill={now ? (running ? "#F5E7D8" : "var(--hot)") : has ? (running ? "#E0C9AF" : own ? "var(--ink)" : "#8B9691") : "none"}
                            stroke={has ? "none" : running ? "#D9B48C" : "var(--rule)"} strokeWidth="1" />;
                        })}
                      </g>
                      {v > 0 && <text x={NW - 7} y="14" textAnchor="end" className="mx-mono" style={{ fontSize: 9, fill: running ? "#F0E2D4" : "var(--muted)" }}>×{v}</text>}
                    </g>
                  );
                })}
              </g>
            </svg>
          </div>
          <div className="mx-leg">
            <span><b style={{ color: "var(--hot)" }}>■</b> executando</span>
            <span>pips: <b>■</b> implementa · <b style={{ color: "#8B9691" }}>■</b> herda de ancestral · ▫ ninguém na cadeia</span>
            <span>×n visitas</span>
            <span>thread: <b style={{ color: "#16232E" }}>TC</b> · <b style={{ color: "#3D6C8C" }}>fundo</b> · <b style={{ color: "#4A6B4F" }}>rede</b></span>
            <span style={{ color: "var(--rf)" }}>--- evento</span>
            <span style={{ color: "var(--ok)" }}>··· por nome</span>
            <span>roda = zoom · arrastar = mover</span>
          </div>
        </div>
        )}

        {execView === "catalog" && (
        <div className="mx-graph">
          <div className="mx-zoom">
            <button className="mx-zbtn" onClick={() => setView((v) => ({ ...v, k: Math.min(4, v.k * 1.2) }))} aria-label="Aproximar">+</button>
            <button className="mx-zbtn" onClick={() => setView((v) => ({ ...v, k: Math.max(0.4, v.k / 1.2) }))} aria-label="Afastar">−</button>
            <button className="mx-zbtn" data-w="1" onClick={() => setView({ k: 1, x: 0, y: 0 })}>ajustar</button>
          </div>
          <div className="mx-svgwrap">
            <svg viewBox={`-10 -10 ${CATALOG_PACKED.totalW + 20} ${CATALOG_PACKED.totalH + 20}`} preserveAspectRatio="xMidYMid meet"
                 onWheel={onWheel} onPointerDown={onDown} onPointerMove={onMove} onPointerUp={onUp} onPointerLeave={onUp}>
              <g transform={`translate(${view.x},${view.y}) scale(${view.k})`} style={{ transformOrigin: "center" }}>
                {CATALOG_PACKED.placed.map((cat) => (
                  <g key={cat.title}>
                    <rect x={cat.ox} y={cat.oy} width={cat.width} height={cat.height} rx="4" fill="#DEE2D8" stroke="var(--rule)" strokeWidth="1" />
                    <text x={cat.ox + CAT_PAD} y={cat.oy + 14} className="mx-mono" style={{ fontSize: 11, fontWeight: 600, fill: "var(--ink)" }}>{cat.title}</text>
                  </g>
                ))}
                {CATALOG_EDGES.map(([a, b]) => {
                  const p = byId[a], q = byId[b];
                  const mid = p.x + CAT_NW + (CAT_COL - CAT_NW) / 2;
                  return <path key={a + b} d={`M ${p.x + CAT_NW} ${p.y} H ${mid} V ${q.y} H ${q.x}`} fill="none" stroke="var(--rule)" strokeWidth="1" />;
                })}
                {CATALOG_ALL.map((n) => {
                  const reg = MODEL[n.cls] ? MODEL[n.cls].r !== false : true;
                  const phases = workPhases(n.cls);
                  const isPinned = pinned === n.id;
                  return (
                    <g key={n.id} className="mx-node" transform={`translate(${n.x},${n.y - CAT_NH / 2})`} onClick={() => setPinned(isPinned ? null : n.id)}>
                      <rect x="0" y="0" width={CAT_NW} height={CAT_NH} rx="2"
                        fill={isPinned ? "#F0EAE2" : "var(--paper)"}
                        stroke={isPinned ? "var(--ink)" : "var(--rule)"} strokeWidth={isPinned ? 1.6 : 1}
                        strokeDasharray={reg ? "0" : "3 2"} opacity={reg ? 1 : 0.7} />
                      <rect x="0" y="0" width="3" height={CAT_NH} fill={THREAD_COLOR[n.thread] || "var(--rule)"} opacity={reg ? 0.9 : 0.5} />
                      <text x="7" y={CAT_NH / 2 + 3.5} className="mx-mono" style={{ fontSize: 10, fontWeight: 600, fill: "var(--ink)", opacity: reg ? 1 : 0.65 }}>{n.cls}</text>
                      <g transform={`translate(${CAT_NW - 42}, ${CAT_NH - 8})`}>
                        {PHASES.map((p) => (
                          <rect key={p.n} x={p.n * 9} y="0" width="6" height="6" rx="1"
                            fill={phases.includes(p.n) ? "#8B9691" : "none"}
                            stroke={phases.includes(p.n) ? "none" : "var(--rule)"} strokeWidth="1" />
                        ))}
                      </g>
                    </g>
                  );
                })}
              </g>
            </svg>
          </div>
          <div className="mx-leg">
            <span>{CATALOG_ALL.filter((n) => MODEL[n.cls] && MODEL[n.cls].r).length} registradas · {CATALOG_ALL.filter((n) => !(MODEL[n.cls] && MODEL[n.cls].r)).length} abstratas/não registradas (borda tracejada)</span>
            <span>pips: <b style={{ color: "#8B9691" }}>■</b> fase implementada na cadeia</span>
            <span>thread: <b style={{ color: "#16232E" }}>TC</b> · <b style={{ color: "#3D6C8C" }}>fundo</b></span>
            <span>roda = zoom · arrastar = mover · clique = detalhe abaixo</span>
          </div>
        </div>
        )}

        <div className="mx-panes">
          {execView === "trace" && (<>
          <div className="mx-pane">
            <div className="mx-card">
              <div className="mx-mono" style={{ fontSize: 12.5, fontWeight: 600, marginBottom: 5 }}>{step.title}</div>
              <p style={{ margin: 0, fontSize: 13, lineHeight: 1.5 }}>{step.body}</p>
              {step.warn && <p className="mx-warn">{step.warn}</p>}
            </div>
            <div className="mx-lbl" style={{ marginTop: 12 }}><span>Pilha de chamadas</span></div>
            {(step.stack || []).map((s, k) => (
              <div key={k} className="mx-mono" style={{ fontSize: 11.5, padding: "2px 0 2px 9px", marginLeft: k * 7, borderLeft: `2px solid ${k === step.stack.length - 1 ? "var(--hot)" : "var(--rule)"}` }}>{s.label}</div>
            ))}
          </div>

          <div className="mx-pane">
            <div className="mx-lbl">
              <span className="mx-mono">{snip ? `${snip.file}:${snip.line}` : ""}</span>
              <span>{snip && snip.trunc ? "truncado" : "C++"}</span>
            </div>
            <div className="mx-code" ref={codeRef}>
              {snip && snip.lines.map((ln, k) => {
                const on = step.hl && k >= step.hl[0] && k <= step.hl[1];
                return <div key={k} className="mx-cl" data-on={on ? 1 : 0}><span className="mx-num">{snip.line + k}</span><span className="mx-src">{ln || " "}</span></div>;
              })}
            </div>
          </div>

          <div className="mx-pane">
            <div className="mx-lbl"><span className="mx-mono">cenario.edl</span><span>{detail.cls} · {detail.edl}</span></div>
            <div className="mx-edl" ref={edlRef}>
              {EDL_TEXT.map((ln, k) => {
                const on = k >= edlRange[0] && k <= edlRange[1];
                return <div key={k} className="mx-cl" data-on={on ? 1 : 0}><span className="mx-num">{k + 1}</span><span className="mx-src">{ln || " "}</span></div>;
              })}
            </div>
          </div>
          </>)}

          <div className="mx-pane">
            <div className="mx-lbl">
              <span className="mx-mono" style={{ color: "var(--ink)", fontWeight: 600 }}>{detail.cls}</span>
              <span>{pinned ? "fixado" : execView === "catalog" ? "clique num nó" : "segue a execução"}</span>
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
            <div style={{ maxHeight: 190, overflow: "auto" }}>
              {slots.map(([s, from], k) => (
                <div className="mx-slot" key={s + k}><span>{s}</span><span>{from}</span></div>
              ))}
              {!slots.length && <div style={{ fontSize: 11.5, color: "var(--muted)" }}>Nenhum slot em toda a cadeia.</div>}
            </div>
            {detail.note && <p className="mx-warn">{detail.note}</p>}
          </div>
        </div>
      </div>

      {execView === "trace" && (
      <div className="mx-transport">
        <button className="mx-btn" data-primary="1" onClick={() => setPlaying((p) => !p)}>{playing ? "Pausar" : "Reproduzir"}</button>
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
        <label style={{ fontSize: 12.3, display: "flex", gap: 5, alignItems: "center" }}>
          <input type="checkbox" checked={showNames} onChange={(e) => setShowNames(e.target.checked)} /> Ligações por nome
        </label>
        <select className="mx-input" value={speed} onChange={(e) => setSpeed(Number(e.target.value))} aria-label="Velocidade">
          <option value={1100}>Lento</option><option value={650}>Normal</option><option value={240}>Rápido</option>
        </select>
      </div>
      )}
    </>
  );
}

/* ---------------------------- catálogo ----------------------------- */

const MOD_ORDER = ["base", "simulation", "terrain", "linkage", "recorder", "models", "interop/dis", "interop/rprfom"];

function Catalog({ onOpen }) {
  const [q, setQ] = useState("");
  const [filter, setFilter] = useState("all");
  const [sel, setSel] = useState(null);
  const boxRef = useRef(null);

  useEffect(() => {
    const h = (e) => { if (e.key === "/" && document.activeElement !== boxRef.current) { e.preventDefault(); boxRef.current && boxRef.current.focus(); } };
    window.addEventListener("keydown", h);
    return () => window.removeEventListener("keydown", h);
  }, []);

  const match = (c) => {
    const e = MODEL[c]; if (!e) return false;
    const t = q.trim().toLowerCase();
    if (t && !(c.toLowerCase().includes(t) || (e.f || "").toLowerCase().includes(t) ||
      (e.sl || []).some((s) => s.toLowerCase().includes(t)))) return false;
    if (filter === "div" && !e.f) return false;
    if (filter === "unreg" && e.r) return false;
    if (filter === "phase" && !e.wp.length) return false;
    if (filter === "scn" && !IN_SCENARIO.has(c)) return false;
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
          {[["all", "Todas"], ["div", "Nome divergente"], ["phase", "Trabalha em fase"], ["unreg", "Não registradas"], ["scn", "No cenário"]].map(([k, l]) => (
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
          <div style={{ maxHeight: 260, overflow: "auto" }}>
            {slots.map(([s, from], k) => <div className="mx-slot" key={s + k}><span>{s}</span><span>{from}</span></div>)}
            {!slots.length && <div style={{ fontSize: 11.5, color: "var(--muted)" }}>EMPTY_SLOTTABLE em toda a cadeia.</div>}
          </div>
        </div>
      </div>
    </div>
  );
}
