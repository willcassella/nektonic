use std::any::Any;
use uuid::Uuid;

pub trait Resource: Any {
}

impl<T: Any> Resource for T {
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct ResourceId(Uuid);

impl ResourceId {
    pub fn new() -> Self {
        ResourceId(Uuid::new_v4())
    }
}

pub struct Metadata {
    pub id: ResourceId,
}
