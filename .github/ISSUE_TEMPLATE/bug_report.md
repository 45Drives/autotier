---
name: Bug report
about: Create a report to help us improve
title: "[BUG]"
labels: bug
assignees: ''

---

**Describe the bug**
A clear and concise description of what the bug is.

**To Reproduce**
- Contents of /etc/autotier.conf
- Command/fstab entry used to mount
- Action that caused error

**Expected behavior**
A clear and concise description of what you expected to happen.

**Error Output**
If applicable, include any error messages shown, including output of `journalctl | grep autotierfs | grep -v "Tiering complete"`.

**System (please complete the following information):**
 - OS: [e.g. Ubuntu 20.04, Arch]
 - Fuse library version [`apt list --installed | grep fuse`]
 - Version [e.g. v1.1.3]

**Additional context**
Add any other context about the problem here.
