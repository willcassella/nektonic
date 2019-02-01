use std::rc::Weak;

use crate::{
    Resource,
    ResourceId,
    Metadata,
    Host,
};

pub trait Consumer {
    fn success(
        self: Box<Self>,
        resource: &Resource,
    );

    fn error(
        self: Box<Self>,
        error: &str,
    );
}

pub trait Mediator {
    fn consume(
        &self,
        _id: ResourceId,
        _consumer: Weak<dyn Consumer>,
    );

    fn consume_on_change(
        &self,
        _id: ResourceId,
        _consumer: Weak<dyn Consumer>
    );

    fn declare_resource(
        &self,
        _id: ResourceId,
    );

    fn run(&mut self);
}
