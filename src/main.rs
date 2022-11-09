use core::ops::Deref;

use std::{env, collections::BTreeSet, io::BufReader};
use std::fs::File;
use std::io::prelude::*;

use postcard::{to_vec, from_bytes};
use heapless::Vec;
use serde::{Serialize, Deserialize};

fn main() {

    let args: Vec<String, 3> = env::args().collect();

    let mut my_db = BTreeSet::new();

    my_db.insert("value");

    match my_db.get("value") {
        Some(val) => println!("got: {}", val),
        None => print!("No value found")
    };

    match unserialized_db.get("value") {
        Some(val) => println!("after unserializing got: {}", val),
        None => print!("No value found")
    };

}

struct DigestiveDatabase<T> {
    _db: BTreeSet<T>,
    _serialized_file: File,
}

impl<T> DigestiveDatabase<T> {
    fn new (name: String) -> DigestiveDatabase<T> {
        let new_db = BTreeSet::new();

        //file that will contain the serialized BTreeSet object as the db
        let mut file = match File::create(name + ".db") {
            Ok(f) => f,
            Err(e) => panic!("{:?}", e)
        };

        //serializing
        let serialized_db: Vec<u8, 50> = match to_vec(&new_db) {
            Ok(db) => db,
            Err(e) => panic!("{}", e)
        };

        //writing in the file
        match file.write_all(serialized_db.as_slice()) {
            Ok(_) => (),
            Err(e) => panic!("Couldn't write in the file: {}", e)
        };

        Self { _db: new_db, _serialized_file: file }
    }

    fn insert (&self, data: T) {
        let mut file_reader = BufReader::new(&self._serialized_file);
        let mut unfiled_db: Vec<u8, 50> = heapless::Vec::new();

        match file_reader.read_exact(&mut unfiled_db) {
            Ok(_) => (),
            Err(e) => panic!("{}", e)
        };

        let unserialized_db: BTreeSet<T> = match from_bytes(&unfiled_db.deref()) {
            Ok(db) => db,
            Err(e) => panic!("{}", e)
        };

        unserialized_db.insert(data);
    }
}

// think to use sled instead of BTreeSet for the db