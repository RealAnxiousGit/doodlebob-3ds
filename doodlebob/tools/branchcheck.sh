cd ~/doodlebob-3ds/cinnamon
B="${DOODLEBOB_DIR:-$(cd "$(dirname "$0")/.." && pwd)}/analysis/builtins.txt"
for br in UNDERTALE-3DS DELTARUNE-3DS DELTARUNE-Chapter-1-3DS experimental main; do
  ref=origin/$br
  has3ds=$(git ls-tree --name-only $ref src/n3ds/ 2>/dev/null | wc -l)
  git show $ref:src/vm_builtins.c > /tmp/vb.c 2>/dev/null
  regs=$(grep -o 'VM_registerBuiltin(ctx, *"[^"]*"' /tmp/vb.c | sed 's/.*"\(.*\)"/\1/' | sort -u)
  missing=$(tr -d '\r' < $B | while read f; do echo "$regs" | grep -qx "$f" || printf "%s " "$f"; done)
  touch=$(git grep -c -E 'hidTouchRead|touchPosition' $ref -- src 2>/dev/null | wc -l)
  echo "$br | date $(git log -1 --format=%cs $ref) | n3ds files $has3ds | builtins $(echo "$regs" | wc -l) | touch files $touch | missing: $missing"
done
rm -f /tmp/vb.c
echo "== preprocessor"; git ls-tree -r --name-only origin/UNDERTALE-3DS tools/n3ds-preprocess | head -20
