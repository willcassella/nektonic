use std::path::PathBuf;
use std::collections::HashMap;
use jib::ResourceId;
use crate::Tag;

#[derive(Clone, Debug)]
pub struct Entry {
    pub id: ResourceId,
    pub path: PathBuf,
    pub tags: Vec<Tag>,
}

pub struct Manifest {
    resources: HashMap<PathBuf, Entry>,
}
