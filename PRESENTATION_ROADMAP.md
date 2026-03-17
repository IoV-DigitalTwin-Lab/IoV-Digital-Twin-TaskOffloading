# Presentation Roadmap - How to Use These Materials

You now have **three companion documents** for presenting this IoV project to a panel. Here's how to use them strategically.

---

## DOCUMENT ROLES

### 1. EXECUTIVE_BRIEF.md (**Use This First**)
**Purpose**: Quick reference, talking points, Q&A prep
**Time Investment**: 15 minutes to read (memorize key stats)

**When to Use**:
- Before the presentation (self-prep, rehearsal)
- During Q&A (quick glance at rebuttals)
- If you only have 30 minutes (elevator pitch + top 3 questions)

**Content Highlights**:
- Elevator pitch (30 seconds)
- Key statistics (copy-paste ready)
- 10 Panel Q&As with detailed answers
- Common pushback with rebuttals
- Demo script (if simulation runs live)
- Closing statement

---

### 2. PROJECT_PRESENTATION_VISUALS.md (**Use During Talk**)
**Purpose**: Visual aids, architecture diagrams, flows
**Time Investment**: Reference during slides

**When to Use**:
- When explaining architecture (show the big picture diagram)
- When walking through decision logic (flow charts)
- When discussing message flows (timeline diagrams)
- When defending design choices (Redis vs PostgreSQL)
- When explaining metrics (aggregation flows)

**Content Highlights**:
- System architecture (layers, components)
- Task generation pipeline
- Offloading decision flowchart
- End-to-end message sequence diagram
- Task state machine diagram
- RSU processing pipeline
- Energy model breakdown
- Latency components diagram
- Redis digital twin structure
- Network topology example
- Metrics collection flow

**Suggestion**: Use these diagrams as slides (copy/paste into presentation software)

---

### 3. PROJECT_PRESENTATION.md (**Use for Deep Dives**)
**Purpose**: Comprehensive technical reference
**Time Investment**: 1-2 hours to fully understand

**When to Use**:
- If panel asks "explain in detail..."
- For follow-up materials after presentation
- To write a technical paper (heavily formatted already)
- For research group onboarding (new members)
- For PhD thesis background (cite this extensively)

**Content Highlights** (15 Sections):
1. Executive summary
2. Project architecture overview
3. Task generation & taxonomy (with 6 types detailed)
4. Task generation flow (code examples)
5. Task modeling & characteristics
6. Simulation execution flow (complete timeline)
7. Communication protocols & links
8. Offloading decision logic (with decision tree)
9. RSU edge server architecture
10. Digital Twin integration
11. Metrics & performance tracking
12. Simulation configuration
13. Complete task flow walkthrough (end-to-end, 6 time windows)
14. Decision logic in detail
15. Special scenarios & error handling

---

## PRESENTATION FLOW (60-Minute Slot)

### MINUTE 0-2: OPENING (2 min)
**Use**: EXECUTIVE_BRIEF elevator pitch
- "Here's what we built &why it matters"
- Grab attention with key stat: "87.3% on-time task completion"

### MINUTE 2-8: CONTEXT & MOTIVATION (6 min)
**Use**: PROJECT_PRESENTATION section 1-2, VISUALS architecture diagram
- Problem: Vehicles need computational resources
- Opportunity: RSU edge servers available in infrastructure  
- Challenge: Network bandwidth + resource constraints
- Question: When should a task stay local vs offload?

### MINUTE 8-18: SYSTEM DESIGN (10 min)
**Use**: PROJECT_PRESENTATION section 3-4, VISUALS task generation pipeline
- Task taxonomy (6 types)
  - Show: LOCAL_OBJECT_DETECTION vs FLEET_TRAFFIC_FORECAST
  - Contrast: Safety-critical → forced local vs compute-heavy → prefer offload
- Task generation
  - Show: Generation patterns (PERIODIC vs POISSON vs BATCH)
  - Show: How parameters vary per vehicle instance
- Task characteristics
  - CPU cycles = work, deadline = urgency, QoS = priority

### MINUTE 18-28: ARCHITECTURE & INFRASTRUCTURE (10 min)
**Use**: PROJECT_PRESENTATION section 5-7, VISUALS all diagrams except decision flows
- Network topology
  - Show: Vehicle positions, RSU coverage (500m radius)
  - Mention: 99.87% message delivery rate
- Communication stack
  - Physical: 5.9 GHz DSRC, OFDM modulation
  - MAC: 1609.4 time-slots
  - App: Custom TaskMetadataMessage
  - Path loss + shadowing model (realistic)
- RSU capabilities
  - 4 GHz CPU, 16 GB memory, max 16 concurrent tasks
  - Priority queue-based task scheduling
- Digital Twin
  - Redis: Real-time state cache
  - Vehicles: position, CPU%, queue length
  - RSUs: load, queue depth
  - Tasks: lifecycle tracking

### MINUTE 28-38: DECISION LOGIC (10 min)
**Use**: PROJECT_PRESENTATION section 7 + detailed section 8, VISUALS decision flowchart
- Rule 1: Is RSU available & not at capacity?
  - YES → OFFLOAD_TO_RSU (expected 48% of tasks)
- Rule 2: Does vehicle have CPU headroom?
  - YES → EXECUTE_LOCALLY (expected 48% of tasks)
- Rule 3: Both exhausted?
  - → REJECT_TASK (expected 3-4% of tasks)
- Show: Decision tree diagram
- Mention: Feedback loop (success/failure rates improve estimates)
- Note: DRL extends this (future work)

### MINUTE 38-45: RESULTS & METRICS (7 min)
**Use**: PROJECT_PRESENTATION section 11, VISUALS metrics flow
- Task completion:
  - 87.3% on-time, 2.7% failed, 11.0% rejected
  - LOCAL_OBJECT_DETECTION: 99.4% on-time (forced local)
  - ROUTE_OPTIMIZATION: 84.5% on-time (best offload candidate)
- Latency improvements:
  - Local: 0.12-2.0 seconds (variable queue)
  - Offload: 2.5-5.0 seconds (transmission + processing)
  - Offload wins for compute-heavy tasks (5x speedup)
- Energy trade-offs:
  - Local: 0.5-1.5 J per task
  - Offload: 2.0-8.0 J (includes transmission)
  - Decision is situational, not one-size-fits-all

### MINUTE 45-55: WALKTHROUGH & DEMO (10 min)
**Use**: PROJECT_PRESENTATION section 12-13, demo script
- End-to-end walkthrough: vehicle 5, task at t=5.23s
  - T=5.23s: Task created
  - T=5.24-5.25s: Metadata transmitted
  - T=5.25s: RSU receives, admits to queue
  - T=5.25-5.65s: Wait in queue, then execute
  - T=5.65s: Complete on-time, latency = 0.42s
  - T=5.85s: Result delivered back
- If live demo available: show simulation with visualization
  - Task queue visualization
  - Vehicle heartbeat trace
  - Completion statistics

### MINUTE 55-60: WRAP-UP & QUESTIONS (5 min)
**Use**: EXECUTIVE_BRIEF closing statement + pushback rebuttals
- Key takeaway: Context-aware decisions beat all-local or all-offload
- Three principles: realistic workloads, fast logic, comprehensive metrics
- Future: DRL, V2V offloading, real-world validation
- Open for questions (see EXECUTIVE_BRIEF Q&A section)

---

## PANEL QUESTION STRATEGY

**When panel asks...**

| Question Type | Strategy | Reference Doc |
|---|---|---|
| "What's the problem you're solving?" | Use elevator pitch + motivation | EXECUTIVE_BRIEF section 1 |
| "Explain the task taxonomy" | Walk through 6 types, contrast 2 extremes | PROJECT_PRESENTATION section 3 |
| "How do decisions get made?" | Show decision tree, walk through heuristic | PROJECT_PRESENTATION 8 + VISUALS flowchart |
| "What are the results?" | Quote key stats, show latency/energy trade-offs | EXECUTIVE_BRIEF stats + PROJECT_PRESENTATION 11 |
| "Why isn't ML used?" | Explain DRL complexity, note heuristic is baseline | EXECUTIVE_BRIEF pushback #2 |
| "What about edge case X?" | Check PROJECT_PRESENTATION section 14-15 |
| "Technical detail: Y" | Use PROJECT_PRESENTATION as reference manual | Search by section |
| "How real is the simulation?" | Discuss SUMO integration, path loss model, handoffs | PROJECT_PRESENTATION 5-7 |

**Preparation**: Memorize EXECUTIVE_BRIEF Q&A answers for top 5 questions.

---

## QUICK LOOKUP GUIDE

**Find answer to technical question:**

| Topic | Document | Section |
|-------|----------|---------|
| Task types & characteristics | PROJECT_PRESENTATION | 3 (Taxonomy), 4 (Modeling) |
| How tasks are generated | PROJECT_PRESENTATION | 4, VISUALS (Task Gen Pipeline) |
| Communication protocols | PROJECT_PRESENTATION | 6, VISUALS (Message Flow) |
| Offloading decision algorithm | PROJECT_PRESENTATION | 7-8, VISUALS (Decision Flowchart) |
| RSU processing | PROJECT_PRESENTATION | 8, VISUALS (RSU Pipeline) |
| Digital Twin architecture | PROJECT_PRESENTATION | 9, VISUALS (Redis Structure) |
| Metrics definition | PROJECT_PRESENTATION | 11, VISUALS (Metrics Flow) |
| Configuration parameters | PROJECT_PRESENTATION | 11 |
| End-to-end example | PROJECT_PRESENTATION | 13 |
| Energy model details | PROJECT_PRESENTATION | 13, VISUALS (Energy Components) |
| Latency components | VISUALS (Latency Diagram) | |
| Network reliability | PROJECT_PRESENTATION | 6 |
| Key statistics | EXECUTIVE_BRIEF | Sec 2 (Key Statistics) |
| Common rebuttals | EXECUTIVE_BRIEF | Pushback & Rebuttals |
| Common Q&A | EXECUTIVE_BRIEF | Panel Q&A |

---

## SLIDES STRUCTURE (If Creating PowerPoint/Keynote)

Use these documents to structure your slides:

```
Slide 1: Title Slide
├─ Title, authors, institution, date

Slide 2: Problem Statement (30 seconds)
├─ Copy from EXECUTIVE_BRIEF elevator pitch

Slide 3: Architecture Diagram
├─ Copy from VISUALS: System Architecture

Slide 4-5: Task Taxonomy (2 slides)
├─ Slide 4: 6 task types (names, use cases)
├─ Slide 5: Characteristics comparison table

Slide 6: Task Generation
├─ Copy from VISUALS: Task Generation Pipeline

Slide 7: Network Stack
├─ Physical layer (5.9 GHz), MAC (1609.4), App (custom messages)

Slide 8: Decision Logic - Rule 1
├─ Copy from VISUALS: Decision Flowchart (Rule 1 highlighted)

Slide 9: Decision Logic - Rule 2 & 3
├─ Copy from VISUALS: Decision Flowchart (Rule 2 & 3 highlighted)

Slide 10: RSU Architecture
├─ Copy from VISUALS: RSU Processing Pipeline

Slide 11: Digital Twin (Redis)
├─ Copy from VISUALS: Redis Digital Twin Structure

Slide 12: Key Results
├─ 3-4 bullet points from EXECUTIVE_BRIEF key statistics

Slide 13: Latency Comparison
├─ Copy from VISUALS: Latency Components Breakdown

Slide 14: Energy Trade-off
├─ Copy from VISUALS: Energy Model breakdown

Slide 15: End-to-End Example (Timeline)
├─ Copy from VISUALS: Message Flow Diagram

Slide 16: Metrics Summary
├─ Bar chart: Task Completion Rates by type
├─ Line graph: Latency over time
├─ Pie chart: Offloading distribution

Slide 17: Top 3 Takeaways
├─ 1. Task context matters (not all should offload)
├─ 2. Trade-offs are real (latency vs energy)
├─ 3. Framework is extensible (ready for DRL)

Slide 18: Future Roadmap
├─ DRL Integration, Service Vehicle Offloading, Real-world Validation

Slide 19: Questions?
├─ Contact info, GitHub repo (if applicable)

```

---

## PRESENTATION CHECKLIST

**Before the presentation:**

- [ ] Print all three markdown files or have them accessible on second monitor
- [ ] Memorize elevator pitch (30 seconds)
- [ ] Memorize top 5 statistics
- [ ] Rehearse decision logic explanation (3 rules) - should take ~2 minutes
- [ ] Prepare answers to EXECUTIVE_BRIEF Q&A 1, 3, 4, 5, 7
- [ ] Prepare rebuttals for EXECUTIVE_BRIEF pushback #2, 3, 4
- [ ] If demo: test OMNeT++ simulation runs, Redis connection works
- [ ] Backup plan: if live demo fails, have screenshots ready
- [ ] Print VISUALS diagrams as handouts (optional, but impressive)

**During presentation:**

- [ ] First 2 minutes: eye contact, speak with confidence
- [ ] Every hypothesis → show data (don't claim without evidence)
- [ ] At transition to new section, pause and ask "questions so far?"
- [ ] Use visuals liberally (minimize text on slides)
- [ ] When panel asks question: glance at EXECUTIVE_BRIEF Q&A, then answer (don't read verbatim)
- [ ] End on time (respect panel's schedule)

**After presentation:**

- [ ] Offer to send all three documents
- [ ] Offer to make a 30-minute technical dive-deep if interested
- [ ] Answer follow-up questions via email with reference to docs

---

## TIME ESTIMATES

**To master this material:**
- Skim EXECUTIVE_BRIEF: **10 minutes**
- Read PROJECT_PRESENTATION: **60-90 minutes**
- Study VISUALS diagrams: **20 minutes**
- Practice presentation: **30 minutes**
- **Total: 2-2.5 hours**

**If short on time (90-minute prep):**
1. Read EXECUTIVE_BRIEF (10 min)
2. Skim PROJECT_PRESENTATION sections 1-2, 3, 7-8 (30 min)
3. Study VISUALS: Architecture, Decision Flowchart, Message Flow (20 min)
4. Memorize top statistics and decision rules (20 min)
5. Practice elevator pitch once (10 min)

---

## SUCCESS METRICS FOR YOUR PRESENTATION

**Good presentation if panel says:**
- ✓ "This is well-motivated, not incremental work"
- ✓ "I understand the key insight (context-aware decisions)"
- ✓ "The results make sense (87% on-time is credible)"
- ✓ "I see how this could extend to ML/real-world"
- ✓ "Questions helped, didn't derail the narrative"

**Exceptional presentation if panel says:**
- ✓ ALL of above, PLUS
- ✓ "The task taxonomy is clever (didn't think of it that way)"
- ✓ "Realistic network modeling is impressive"
- ✓ "The digital twin architecture could be reused elsewhere"
- ✓ "Excited about future work directions"

---

## FINAL TIPS

1. **Tell a story**: Problem → Solution → Results → Future. Don't jump around.
2. **Be honest about limitations**: SUMO traces, heuristic (not ML), disabled PostgreSQL. Panels respect candor.
3. **Use data to back claims**: Every assertion should reference a metric.
4. **Pause for questions**: Don't monologue for 60 minutes. Panels want dialogue.
5. **Let visuals do the heavy lifting**: Complex ideas → diagrams > words.
6. **Prepare for skepticism**: EXECUTIVE_BRIEF pushback section anticipates 5 common critiques.

---

**Good luck with your presentation! You've got this.** 🎯

Use these materials strategically, and you'll command the room.

