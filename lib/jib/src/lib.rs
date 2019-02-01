mod mediator;
pub use mediator::Mediator;

mod host;
pub use host::Host;

mod resource;
pub use resource::{
    Resource,
    ResourceId,
    Metadata,
};

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
