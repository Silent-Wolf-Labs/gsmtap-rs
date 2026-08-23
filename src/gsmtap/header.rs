pub const GSMTAP_VERSION: u8 = 2;
pub(crate) const BASE_HEADER_LENGTH: usize = 16;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct GsmtapHeader {
    version: u8,
    header_length_words: u8,
    message_type: u8,
    timeslot: u8,
    arfcn: u16,
    signal_dbm: i8,
    snr_db: i8,
    frame_number: u32,
    subtype: u8,
    antenna_number: u8,
    sub_slot: u8,
    reserved: u8,
}

impl GsmtapHeader {
    // GSMTAP has one fixed field per header slot; keeping this constructor
    // field-for-field makes packet construction explicit for API callers.
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        version: u8,
        header_length_words: u8,
        message_type: u8,
        timeslot: u8,
        arfcn: u16,
        signal_dbm: i8,
        snr_db: i8,
        frame_number: u32,
        subtype: u8,
        antenna_number: u8,
        sub_slot: u8,
        reserved: u8,
    ) -> Self {
        Self {
            version,
            header_length_words,
            message_type,
            timeslot,
            arfcn,
            signal_dbm,
            snr_db,
            frame_number,
            subtype,
            antenna_number,
            sub_slot,
            reserved,
        }
    }
    pub fn version(&self) -> u8 {
        self.version
    }
    pub fn header_length_words(&self) -> u8 {
        self.header_length_words
    }
    pub fn header_length(&self) -> usize {
        usize::from(self.header_length_words) * 4
    }
    pub fn message_type(&self) -> u8 {
        self.message_type
    }
    pub fn timeslot(&self) -> u8 {
        self.timeslot
    }
    pub fn arfcn(&self) -> u16 {
        self.arfcn
    }
    pub fn signal_dbm(&self) -> i8 {
        self.signal_dbm
    }
    pub fn snr_db(&self) -> i8 {
        self.snr_db
    }
    pub fn frame_number(&self) -> u32 {
        self.frame_number
    }
    pub fn subtype(&self) -> u8 {
        self.subtype
    }
    pub fn antenna_number(&self) -> u8 {
        self.antenna_number
    }
    pub fn sub_slot(&self) -> u8 {
        self.sub_slot
    }
    pub fn reserved(&self) -> u8 {
        self.reserved
    }
}
