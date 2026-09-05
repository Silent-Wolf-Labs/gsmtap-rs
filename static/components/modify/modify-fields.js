export { editableFieldLabels, editableHeaderFields, fieldGroups, fieldTooltips, hexFieldNames, packetFieldNames } from '../packet/packet-schema.js';

export const numericFieldRanges = {
  version: [0, 255], headerLengthWords: [4, 255], messageType: [0, 255], timeslot: [0, 255],
  arfcn: [0, 65535], signalDbm: [-128, 127], snrDb: [-128, 127], frameNumber: [0, 4294967295],
  subtype: [0, 255], antennaNumber: [0, 255], subSlot: [0, 255], reserved: [0, 255],
};
