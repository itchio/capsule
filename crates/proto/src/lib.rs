mod proto_capnp {
  include!(concat!(env!("OUT_DIR"), "/proto_capnp.rs"));
}
pub use proto_capnp::*;
