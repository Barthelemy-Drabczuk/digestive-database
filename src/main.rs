use core::ops::Deref;

use std::{collections::BTreeSet, io::BufReader};
use std::fs::File;
use std::io::prelude::*;

use postcard::{to_vec, from_bytes};
use heapless::Vec;

fn main() {
    let mut my_db = BTreeSet::new();

    my_db.insert("value");

    match my_db.get("value") {
        Some(val) => println!("got: {}", val),
        None => print!("No value found")
    };

    let serialized_db: Vec<u8, 50> = match to_vec(&my_db) {
        Ok(db) => db,
        Err(e) => panic!("{}", e)
    };

    let mut file = match File::create("my_db.db") {
        Ok(f) => f,
        Err(e) => panic!("{:?}", e)
    };

    match file.write_all(serialized_db.as_slice()) {
        Ok(_) => (),
        Err(e) => panic!("Couldn't write in the file: {}", e)
    };

    let mut file_reader = BufReader::new(file);
    let mut unfiled_db: Vec<u8, 50> = heapless::Vec::new();

    match file_reader.read_exact(&mut unfiled_db) {
        Ok(_) => (),
        Err(e) => panic!("{}", e)
    };

    let unserialized_db: BTreeSet<&str> = match from_bytes(&unfiled_db.deref()) {
        Ok(db) => db,
        Err(e) => panic!("{}", e)
    };

    match unserialized_db.get("value") {
        Some(val) => println!("after unserializing got: {}", val),
        None => print!("No value found")
    };

}
