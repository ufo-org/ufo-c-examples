# ufo-c-examples

## Troubleshooting

```
thread '<unnamed>' panicked at 'called `Result::unwrap()` on an `Err` value: SystemError(Sys(EPERM))', .../src/ufo_core.rs:215:14
```

System is set up only to allow root use `userfaultf`. Fix by setting system
priivileges to allow unprivileged users to use `userfaultfd`:

```bash
sysctl -w vm.unprivileged_userfaultfd=1
```
