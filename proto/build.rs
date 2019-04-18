extern crate capnpc;

fn main() {
    ::capnpc::CompilerCommand::new().edition(::capnpc::RustEdition::Rust2018).file("proto.capnp").run().unwrap();
}