pub fn get(name: &str) -> Option<String> {
  // yep, that's pretty dumb. and yet it's the only
  // way to retrieve the environment variable we want
  // when injected into bash (of all things?) because
  // getenv returns NULL for CAPSULE_PORT, RUST_LOG, etc.
  for (k, v) in std::env::vars() {
    if k == name {
      return Some(v);
    }
  }
  None
}
