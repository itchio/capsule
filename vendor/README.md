# vendored dependencies

The process for updating a vendored dependency goes something like this:

```bash
butler wipe shoom
butler ditto ~/Dev/shoom ./shoom
(cd shoom && git rev-parse HEAD) >! shoom.version
butler wipe shoom/.git
```
