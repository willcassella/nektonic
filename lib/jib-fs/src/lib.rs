mod manifest;
pub use manifest::Manifest;

mod tag;
pub use tag::Tag;

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
