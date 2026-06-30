# GitHub issue: OCCT thread-safety crash in parallel ray projection

Copy each section below into GitHub as a separate post.

---

## POST 1 — open the issue

**Title:** SIGILL crash in parallel `ray_bidirectional` projection (OCCT thread-unsafe lazy cache)

---

### Description

Running UV mapping with `method=ray_bidirectional` and `enable_parallel=True` crashes
the Python process with `SIGILL` (illegal instruction). The crash happens inside
OpenCASCADE's `libTKGeomAlgo`, not in Python or application code.
A second run after the crash caused a full system kernel panic and reboot.

### Environment

- OS: Linux (Arch), kernel 6.16.1
- Python: 3.12 (conda env `game-qt6-py312`)
- OpenCASCADE: 7.8.1
- Crash confirmed: 2026-06-29

### Steps to reproduce

```python
from cad_uv_map import MappingMethod, describe_shape_faces, map_source_samples_to_target
from cad_uv_map.api import MappingContext
from tests.fixtures.cad_cases import pedestal_ribs_nofold_pair

pair = pedestal_ribs_nofold_pair()
faces = describe_shape_faces(pair.low)
face = next(f for f in faces if abs(f.normal_at(f.center()).Z) < 0.5)

samples = [(u, v) for v in [0.25, 0.5, 0.75] for u in [0.25, 0.5, 0.75]]

ctx = MappingContext()
ctx.enable_parallel = True

# This crashes the process:
map_source_samples_to_target(
    pair.low, pair.high, face.face_id, samples, MappingMethod.ray_bidirectional, ctx
)
```

### Crash evidence

**How to collect these logs** (run immediately after a crash, before reboot):

```bash
# 1. Kernel trap line — confirms signal, library, and instruction pointer
journalctl -b 0 --no-pager -o short-iso | grep -E "traps|coredump|signal 4" \
    > collapse/kernel-trap.txt

# 2. Core dump summary
coredumpctl info > collapse/core-info.txt

# 3. Full thread stack traces from the core dump
coredumpctl debug --debugger-arguments="-batch -ex 'thread apply all bt'" \
    > collapse/gdb-threads.txt
```

---

**1. Kernel log — trap line**

```
Jun 29 19:28:18 archlinux kernel: traps: python[173476] trap invalid opcode \
    ip:7f8d278ab3da sp:7f8c9e7fb280 error:0 \
    in libTKGeomAlgo.so.7.8.1[2ab3da,7f8d276b7000+4e2000]
Jun 29 19:28:18 archlinux systemd-coredump[173477]: \
    Process 173372 (python) of user 1000 terminated abnormally with signal 4/ILL
```

`trap invalid opcode` in `libTKGeomAlgo.so.7.8.1` is the kernel's record of the
SIGILL. The instruction pointer `2ab3da` (offset into the library) points into
`IntCurveSurface_ThePolyhedronOfHInter::FillBounding`, confirmed by the GDB trace below.

---

**2. Core dump info summary**

```
Signal:        4 (ILL)
PID:           173372
Command Line:  python visualize_qt.py --case pedestal_ribs_nofold \
                   --method ray_bidirectional --force
Executable:    .../envs/game-qt6-py312/bin/python3.12
Size on Disk:  112.1M
```

---

**3. GDB thread stacks — crashing thread**

Two worker threads were both inside `IntCurvesFace_ShapeIntersector::Load()` on
the same face at the time of the crash. Thread A faulted in `FillBounding`,
Thread B was in `BRepAdaptor_Curve2d::Initialize` on the same face.

Thread A (faulted):
```
#0  IntCurveSurface_ThePolyhedronOfHInter::FillBounding()
        (libTKGeomAlgo.so.7.8.1 + 0x2ab3da)   ← invalid opcode here
#1  IntCurveSurface_ThePolyhedronOfHInter::Init(Adaptor3d_Surface, ...)
        (libTKGeomAlgo.so.7.8.1)
#2  IntCurveSurface_ThePolyhedronOfHInterC2(Adaptor3d_Surface, ...)
        (libTKGeomAlgo.so.7.8.1)
#3  IntCurvesFace_Intersector::IntCurvesFace_Intersector(TopoDS_Face, ...)
        (libTKTopAlgo.so.7.8.1)
#4  IntCurvesFace_ShapeIntersector::Load(TopoDS_Shape, tol)
        (libTKTopAlgo.so.7.8.1)
#5  [project_ray_to_face_impl — _native.cpython-312-x86_64-linux-gnu.so]
#6  [map_low_face_sample_to_high_faces_ray — _native]
#7  [parallel lambda / sample chunk — _native]
#8  [std::async worker — _native]
#9  std::__future_base::_State_baseV2::_M_do_set(...)
#13 execute_native_thread_routine (libstdc++.so.6)
```

Thread B (concurrent, same call site):
```
#0  libTKMath.so.7.8.1
#1  BRep_Tool::CurveOnSurface(TopoDS_Edge, Geom_Surface, ...)
        (libTKBRep.so.7.8.1)
#2  BRepAdaptor_Curve2d::Initialize(TopoDS_Edge, TopoDS_Face)
        (libTKBRep.so.7.8.1)
#3  BRepAdaptor_Curve2d::BRepAdaptor_Curve2d(TopoDS_Edge, TopoDS_Face)
        (libTKBRep.so.7.8.1)
#7  IntCurvesFace_Intersector::IntCurvesFace_Intersector(TopoDS_Face, ...)
        (libTKTopAlgo.so.7.8.1)
#8  IntCurvesFace_ShapeIntersector::Load(TopoDS_Shape, tol)
        (libTKTopAlgo.so.7.8.1)
#9  [project_ray_to_face_impl — _native.cpython-312-x86_64-linux-gnu.so]
```

Both threads entered `Load()` on the same `TopoDS_Face` handle simultaneously.
The face's shared geometry was mutated by one thread while the other was reading it.

### Affected methods

- `ray_bidirectional` — confirmed crash
- `ray` — same code path, same risk, has not crashed yet
- `nearest` — different code path (`BRepExtrema_DistShapeShape`), also suspected unsafe

---

## POST 2 — root cause (add as first comment)

### Root cause analysis

**Starting from the kernel trap line**

```
traps: python[173476] trap invalid opcode ip:7f8d278ab3da
    in libTKGeomAlgo.so.7.8.1[2ab3da, ...]
```

`trap invalid opcode` means the CPU tried to execute bytes that do not correspond
to any valid x86 instruction. This is not a logic error or a wrong value —
it means memory that was supposed to contain executable code contained garbage
at the moment it was executed. That is a strong indicator of memory corruption
at runtime, not a bug in the algorithm logic itself.

The fault is inside `libTKGeomAlgo`, not in our code. So something our code
did caused OCCT's memory to become corrupted.

---

**Reading the two thread stacks together**

The GDB output shows two worker threads alive at the time of the crash.
Both stacks reach the same call site — `IntCurvesFace_ShapeIntersector::Load()` —
and both originate from `std::async` workers spawned by our `_native` extension:

Thread A (the one that faulted):
```
IntCurveSurface_ThePolyhedronOfHInter::FillBounding   ← invalid opcode here
IntCurveSurface_ThePolyhedronOfHInter::Init(surface)
IntCurveSurface_ThePolyhedronOfHInterC2(surface, ...)
IntCurvesFace_Intersector(TopoDS_Face, ...)
IntCurvesFace_ShapeIntersector::Load(TopoDS_Face, tol)
[_native — our project_ray_to_face_impl]
[_native — std::async parallel lambda]
```

Thread B (concurrent, same call site, different depth):
```
BRep_Tool::CurveOnSurface(TopoDS_Edge, Geom_Surface, ...)
BRepAdaptor_Curve2d::Initialize(TopoDS_Edge, TopoDS_Face)
BRepAdaptor_Curve2d(TopoDS_Edge, TopoDS_Face)
IntCurvesFace_Intersector(TopoDS_Face, ...)
IntCurvesFace_ShapeIntersector::Load(TopoDS_Face, tol)
[_native — our project_ray_to_face_impl]
[_native — std::async parallel lambda]
```

The two threads were both inside `Load()` at the same time. Thread A had
progressed further (already building the internal polyhedron approximation),
Thread B was still initializing the face's 2D trim curve adaptor.

---

**Why this proves shared state**

Both threads entered `Load()` from our parallel sample-chunk code in
`projection_ray.cpp`. In that code, every thread iterates over the same
`high_faces` vector and calls `Load(high_faces[i], tol)` for every face `i`.

`TopoDS_Face` is a handle — a reference-counted pointer. Passing it by value
to each thread copies the pointer, not the underlying object. Both threads
were therefore calling `Load()` on the exact same `BRep_TFace` and `Geom_Surface`
at the same time.

`IntCurvesFace_Intersector::Load()` builds an internal triangulated approximation
of the face geometry (`IntCurveSurface_ThePolyhedronOfHInter`) and reads the
face's 2D trim curves (`BRepAdaptor_Curve2d`). Both operations read from and
write to fields inside the shared geometry object — OCCT geometry objects use
lazy initialization, computing and caching internal data on first access.

Thread A and Thread B raced to perform that initialization on the same object.
One thread's write corrupted data that the other thread was about to execute
as code, producing the `trap invalid opcode`.

---

**Why `ray_bidirectional` and not `ray` or `nearest`**

`ray_bidirectional` calls the ray-intersection path twice per sample (outward
then inward). This doubles the number of concurrent `Load()` calls per unit
time, widening the window in which two threads can collide on the same face.
`ray` has the same vulnerability but a narrower window — it has not crashed yet.

`nearest` uses `BRepExtrema_DistShapeShape` instead of
`IntCurvesFace_ShapeIntersector`. This traverses different internal OCCT paths
and triggers different lazy initialization. The race window is narrower still,
but the underlying sharing problem is identical — it is not safe, it has just
not manifested yet.

---

**Note on the kernel panic**

A second run after the first crash caused a full system panic and reboot,
with journal logs not flushed before the machine died. This is consistent
with the same memory corruption reaching further — once one region of OCCT's
heap is corrupted, a subsequent run can corrupt adjacent memory including
kernel-mapped pages, triggering a hardware fault that the kernel cannot recover
from. This is a known consequence of data races in native extensions that
share memory with the kernel's address space.

---

## POST 3 — fix (add after implementation is complete)

### Fix: [brief description of chosen plan]

<!--
Fill in after implementing. Template:

### What was changed

- Added `PreparedFace` struct in `cpp/src/projection/prepared_face.hpp`:
  extracts poles, knots, and trim data from a `TopoDS_Face` into plain
  C++ arrays that carry no shared OCCT handles.
- Added `prepare_face(TopoDS_Face)` function called once per face in serial,
  before the `std::async` loop.
- Replaced `IntCurvesFace_ShapeIntersector` calls with `intersect_ray(PreparedFace)`
  which operates only on the extracted plain data.
- Applied the same change to `projection_nearest.cpp`.

### Why this is safe

Each `PreparedFace` is owned by the caller and read-only after construction.
No OCCT handles are accessed during the parallel phase. Two threads may call
`intersect_ray()` on different `PreparedFace` objects simultaneously with no
shared state.

### Test results

- 17 existing tests: pass
- Crash regression (`test_crash_regression_ray_bidirectional_pedestal_nofold`): pass
- Correctness (parallel == serial, 6 cases): pass
- Stress (200 iterations × 3 methods): pass
- ThreadSanitizer run: no data race reports from cad_uv_map code

### Commit

[link to commit]
-->
