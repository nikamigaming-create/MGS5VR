(() => {
  "use strict";
  const data = window.FIELD_GUIDE_DATA;
  if (!data || !data.bindings || !data.coverage) {
    document.body.innerHTML = "<main><h1>Guide data is missing</h1><p>Run tools/field-guide/build_guide.py.</p></main>";
    return;
  }
  const $ = (id) => document.getElementById(id);
  const actionByName = new Map(data.bindings.actions.map((item) => [item.name, item]));
  const axisByName = new Map(data.bindings.axes.map((item) => [item.name, item]));
  const records = data.coverage.records;
  const families = [
    ["all", "All situations"], ["title_and_cabin", "Title & cabin"], ["menus", "Menus"],
    ["on_foot", "On foot"], ["equipment", "Equipment & items"], ["optics", "Optics"],
    ["buddy_and_mounts", "Buddies & mounts"], ["mounted_vehicle_roles", "Vehicles & mounted"],
    ["helicopter_transport", "Helicopter transport"],
    ["cinematic_and_interactive_sequences", "Cinematics & lessons"],
    ["game_adapter_boundary", "GZ button mode"]
  ];
  const familyName = new Map(families);
  const outcomeName = {
    verified_bounded: "Observed in simulator · bounded", partial_observation: "Partly observed",
    observed_failure: "Run reported failure", unverified: "Not yet tested", unknown: "Outcome unknown"
  };
  const pretty = {
    a:"A", b:"B", x:"X", y:"Y", menu:"MENU", left_stick_click:"L CLICK", right_stick_click:"R CLICK",
    left_grip:"L GRIP", right_grip:"R GRIP", left_trigger:"L TRIGGER", right_trigger:"R TRIGGER",
    left_stick:"L STICK", right_stick:"R STICK", left_stick_up:"L UP", left_stick_down:"L DOWN",
    left_stick_left:"L LEFT", left_stick_right:"L RIGHT", right_stick_up:"R UP", right_stick_down:"R DOWN",
    right_stick_left:"R LEFT", right_stick_right:"R RIGHT", left_thumbrest:"L THUMBREST", right_thumbrest:"R THUMBREST"
  };
  const actionDescriptions = {
    "system.idroid":"Open the iDroid",
    "system.pause":"Open the Pause menu",
    "system.recenter":"Recenter your view",
    "system.toggle_vr":"Turn the VR view on or off",
    "system.native_buttons":"Switch between gamepad controls and VR controls",
    "gameplay.run":"Run or sprint",
    "gameplay.stance":"Change posture: stand, crouch, or prone",
    "gameplay.dive":"Dive or roll",
    "gameplay.interact":"Interact at a prompt",
    "gameplay.reload":"Reload the current weapon",
    "gameplay.ready_weapon":"Raise or lower your weapon",
    "gameplay.pickup_carry":"Pick up or carry",
    "gameplay.support_grip":"Hold as an alternate-action modifier",
    "gameplay.switch_weapon":"Switch weapons",
    "gameplay.zoom":"Adjust weapon zoom",
    "gameplay.fire_or_cqc":"Fire or use contextual CQC",
    "gameplay.equip_binoculars":"Raise binoculars",
    "gameplay.native_a":"Use the game's A button",
    "gameplay.native_x":"Use the game's X button",
    "gameplay.native_left_shoulder":"Use the game's left shoulder button",
    "gameplay.native_right_shoulder":"Use the game's right shoulder button",
    "gameplay.native_right_click":"Click the game's right stick",
    "gameplay.native_dpad_up":"Press up on the game's D-pad",
    "gameplay.native_dpad_down":"Press down on the game's D-pad",
    "gameplay.native_dpad_left":"Press left on the game's D-pad",
    "gameplay.native_dpad_right":"Press right on the game's D-pad",
    "equipment.open":"Open the equipment picker",
    "equipment.primary":"Choose the primary weapon slot",
    "equipment.secondary":"Choose the secondary weapon slot",
    "equipment.support":"Choose the support-weapon slot",
    "equipment.items":"Choose the item slot",
    "equipment.back":"Go back or close the picker",
    "equipment.use":"Use the selected item",
    "commands.open":"Open the buddy command menu",
    "commands.keep_open":"Keep the buddy menu open",
    "commands.mounted_open":"Open mounted buddy commands",
    "commands.mounted_keep_open":"Keep mounted commands open",
    "commands.confirm":"Choose the highlighted command",
    "commands.back":"Go back or close commands",
    "binoculars.stow":"Lower binoculars",
    "binoculars.zoom":"Zoom binoculars",
    "binoculars.mark":"Mark a target",
    "binoculars.clear_mark":"Clear a target mark",
    "binoculars.support_grip":"Hold as an alternate-action modifier",
    "binoculars.run":"Run with binoculars raised",
    "binoculars.dive":"Dive or roll with binoculars raised",
    "binoculars.stance":"Change posture with binoculars raised",
    "menus.confirm":"Select the highlighted menu item",
    "menus.back":"Go back or close the menu",
    "menus.action_x":"Use menu action X",
    "menus.action_y":"Use menu action Y",
    "menus.previous_tab":"Move to the previous tab",
    "menus.next_tab":"Move to the next tab",
    "menus.left_trigger":"Use the left trigger in menus",
    "menus.right_trigger":"Use the right trigger in menus",
    "menus.left_click":"Click the left stick in menus",
    "menus.right_click":"Click the right stick in menus",
    "menus.dpad_up":"Move menu focus up",
    "menus.dpad_down":"Move menu focus down",
    "menus.dpad_left":"Move menu focus left",
    "menus.dpad_right":"Move menu focus right",
    "horse.gallop":"Change horse speed",
    "horse.interact":"Interact while mounted",
    "horse.stance":"Change posture while mounted",
    "horse.reload":"Reload while mounted",
    "vehicle.accelerate":"Accelerate",
    "vehicle.brake_reverse":"Brake or reverse",
    "vehicle.interact":"Interact from the vehicle",
    "vehicle.weapon_or_call":"Use the vehicle weapon or call",
    "vehicle.native_a":"Use A in the vehicle",
    "vehicle.native_b":"Use B in the vehicle",
    "native.a":"Press A",
    "native.b":"Press B",
    "native.x":"Press X",
    "native.y":"Press Y",
    "native.back":"Press Back",
    "native.start":"Press Start",
    "native.dpad_up":"Press up on the D-pad",
    "native.dpad_down":"Press down on the D-pad",
    "native.dpad_left":"Press left on the D-pad",
    "native.dpad_right":"Press right on the D-pad",
    "native.left_click":"Click the left stick",
    "native.right_click":"Click the right stick",
    "native.left_shoulder":"Press the left shoulder button",
    "native.right_shoulder":"Press the right shoulder button",
    "native.left_trigger":"Press the left trigger",
    "native.right_trigger":"Press the right trigger",
    "turn.left":"Turn left",
    "turn.right":"Turn right"
  };
  const nodes = {
    left:["menu","x","y","left_stick","left_stick_click","left_stick_up","left_stick_down","left_stick_left","left_stick_right","left_grip","left_trigger","left_thumbrest"],
    right:["a","b","right_stick","right_stick_click","right_stick_up","right_stick_down","right_stick_left","right_stick_right","right_grip","right_trigger","right_thumbrest"]
  };
  let chosenFamily = "all";
  let chosenRecord = records.find((r) => r.id === "TPP.FOOT.POSTURE_LOCOMOTION") || records[0];
  let zoomed = false;
  let activeVideoKind = "posture";

  function esc(value) {
    return String(value === undefined || value === null ? "" : value).replace(/[&<>"']/g, (c) =>
      ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
  }
  function title(id) { return id.split(".").slice(1).join(" · ").replace(/_/g, " "); }
  function statusClass(value) {
    if (value === "verified_bounded" || value === "partial_observation") return "observed";
    if (value === "observed_failure") return "failure";
    if (value === "unknown") return "unknown";
    return "";
  }
  function wasObserved(value) {
    return value === "verified_bounded" || value === "partial_observation" || value === "observed_failure";
  }
  function actionsFor(record) {
    return Array.from(new Set(record.required_actions.flatMap((x) => (x.binding_refs && x.binding_refs.actions) || [])));
  }
  function bindingLabel(record) {
    const names = actionsFor(record);
    if (names.length) {
      const mapped = names.filter((name) => {
        const item = actionByName.get(name);
        return item && item.bindings && item.bindings.length;
      }).length;
      if (mapped === names.length) return "Binding listed";
      if (mapped) return "Part of route listed";
      return "No binding in selected file";
    }
    if (axesFor(record).some((name) => {
      const item = axisByName.get(name);
      return item && item.source && item.source !== "disabled";
    })) return "Movement mapped";
    if (record.required_actions.some((item) => item.direct_route)) return "Direct input documented";
    return "No binding listed";
  }
  function directInputNote(record) {
    const routes = record.required_actions.map((item) => item.direct_route).filter(Boolean);
    if (!routes.length) return "";
    return '<div class="direct-route-note"><b>Direct input noted in the situation record</b>' +
      routes.map((route) => {
        const hand = route.controller === "left" ? "Left" : route.controller === "right" ? "Right" : route.controller;
        const seconds = route.duration_seconds ? Number(route.duration_seconds).toFixed(2) + " seconds" : "duration not listed";
        const sourceNote = route.in_effective_binding_export
          ? "listed in the selected controls file"
          : "documented for the walkthrough, outside the remappable controls file";
        return '<p>' + esc(hand) + ' controller · ' + esc(route.component || "input") + ' · ' +
          esc(seconds) + ' · ' + esc(sourceNote) + '</p>';
      }).join("") + '</div>';
  }
  function axesFor(record) {
    return Array.from(new Set(record.required_actions.flatMap((x) => (x.binding_refs && x.binding_refs.axes) || [])));
  }
  function usedInputs(record) {
    const inputs = new Set();
    actionsFor(record).forEach((name) => {
      const item = actionByName.get(name);
      (item ? item.bindings : []).forEach((binding) => binding.inputs.forEach((token) => inputs.add(token)));
    });
    axesFor(record).forEach((name) => {
      const source = (axisByName.get(name) || {}).source;
      if (source && source !== "disabled") inputs.add(source);
    });
    return inputs;
  }
  function renderModes() {
    const root = $("mode-list");
    root.innerHTML = families.map((pair, index) => {
      const count = pair[0] === "all" ? records.length : records.filter((r) => r.family === pair[0]).length;
      return '<button class="mode-button" type="button" data-family="' + esc(pair[0]) + '" aria-pressed="' +
        (chosenFamily === pair[0]) + '"><span><span class="mode-num">' + String(index).padStart(2, "0") +
        "</span> " + esc(pair[1]) + '</span><span class="mode-count">' + count + "</span></button>";
    }).join("");
  }
  function matchesSearch(record, query) {
    if (!query) return true;
    const terms = [record.id,record.context,record.family,record.game,record.binding_status,record.native_outcome_status,
      JSON.stringify(record.identity),JSON.stringify(record.required_actions),JSON.stringify(record.exit_or_cancel)];
    actionsFor(record).forEach((name) => {
      const item = actionByName.get(name);
      terms.push(name,item && item.label);
      (item ? item.bindings : []).forEach((b) => terms.push(...b.inputs));
    });
    axesFor(record).forEach((name) => terms.push(name,(axisByName.get(name) || {}).source));
    return terms.join(" ").toLowerCase().includes(query);
  }
  function visibleRecords() {
    const game = $("game-filter").value;
    const query = $("search").value.trim().toLowerCase();
    const observedOnly = $("observed-filter").checked;
    return records.filter((r) =>
      (chosenFamily === "all" || r.family === chosenFamily) &&
      (game === "all" || r.game === game || (game === "GZ" && r.game.startsWith("GZ"))) &&
      (!observedOnly || wasObserved(r.native_outcome_status)) && matchesSearch(r, query));
  }
  function renderRecords() {
    const rows = visibleRecords();
    $("list-heading").textContent = familyName.get(chosenFamily) || "All situations";
    $("result-count").textContent = rows.length + " SITUATION" + (rows.length === 1 ? "" : "S");
    $("empty-state").hidden = rows.length > 0;
    $("record-list").innerHTML = rows.map((r) =>
      '<button class="record-card" type="button" data-record="' + esc(r.id) + '" aria-pressed="' +
      (chosenRecord && chosenRecord.id === r.id) + '"><span class="record-card-top"><span><strong>' + esc(title(r.id)) +
      '</strong></span><span class="game-tag">' + esc(r.game) +
      '</span></span><p>' + esc(r.context) + '</p><span class="record-meta"><span class="status-pill ' +
      statusClass(r.native_outcome_status) + '">' + esc(outcomeName[r.native_outcome_status] || r.native_outcome_status) +
      '</span><span class="tag">' + esc(bindingLabel(r)) + "</span></span></button>"
    ).join("");
    let rowsClick = $("record-list");
    if (!rowsClick.dataset.wired) {
      rowsClick.dataset.wired = "true";
      rowsClick.addEventListener("click", (event) => {
        const button = event.target.closest("[data-record]");
        if (!button) return;
        chosenRecord = records.find((r) => r.id === button.dataset.record);
        zoomed = false;
        renderRecords();
      });
    }
    if (!rows.some((r) => chosenRecord && r.id === chosenRecord.id) && rows.length) {
      chosenRecord = rows[0];
      renderRecords();
      return;
    }
    if (!rows.length) $("detail").innerHTML = '<div class="empty-state">Choose another mode or clear a search filter.</div>';
    else renderDetail();
  }
  function renderPredicate(identity) {
    const predicate = identity && identity.predicate;
    if (!predicate) return '<p class="plain-copy">No executable entry predicate is available. Treat this context as a documentation lead, not a ready-to-run scenario.</p>';
    const parts = [];
    Object.entries(predicate.all || {}).forEach(([key,value]) => parts.push(key + " = " + JSON.stringify(value)));
    (predicate.any || []).forEach((option,index) =>
      Object.entries(option).forEach(([key,value]) => parts.push("any " + (index + 1) + ": " + key + " = " + JSON.stringify(value))));
    return '<div class="predicate">' + (parts.length ? parts.map((x) => "<code>" + esc(x) + "</code>").join("") : "<code>No supported fields</code>") + "</div>";
  }
  function gesture(binding) {
    const values = {
      level:"LEVEL / HELD", press:"PRESS", release:"RELEASE",
      tap:"TAP · " + binding.milliseconds + " MS", hold:"HOLD · " + binding.milliseconds + " MS"
    };
    return values[binding.gesture] || String(binding.gesture).toUpperCase();
  }
  function actionRoute(name) {
    const item = actionByName.get(name);
    if (!item) return '<div class="route"><div class="route-name"><b>' + esc(actionDescriptions[name] || "In-game action") +
      '</b><small class="route-ref">' + esc(name) + '</small></div><div class="route-detail"><span class="unbound">NOT IN SELECTED CONTROLS FILE</span></div></div>';
    const bindings = item.bindings || [];
    const lines = bindings.length ? bindings.map((b) =>
      '<div class="binding-line"><span class="gesture">' + esc(gesture(b)) + "</span>" +
      b.inputs.map((token) => '<span class="input-chip" title="Exported token: ' + esc(token) + '">' +
        esc(pretty[token] || token) + "</span>").join("") + "</div>").join("") :
      '<span class="unbound">UNBOUND IN THIS EXPORT</span>';
    return '<div class="route"><div class="route-name"><b>' + esc(actionDescriptions[name] || "In-game action") + '</b>' +
      '<small class="route-ref">' + esc(name) + '</small></div><div class="route-detail">' +
      '<div class="route-label">' + esc(item.label || "No configured label") + '</div>' +
      (item.modifier ? '<span class="route-label">Modifier input</span>' : "") +
      '<div class="route-bindings">' + lines + "</div></div></div>";
  }
  function renderAxes(record) {
    const axes = axesFor(record);
    if (!axes.length) return '<p class="plain-copy">No movement axes are listed for this situation.</p>';
    return '<div class="axis-list">' + axes.map((name) => {
      const item = axisByName.get(name);
      const label = name.replace(/^axes\./, "").replace(/_/g, " ");
      return '<span class="axis-chip"><b>' + esc(label) + "</b> → " + esc(item ? item.source : "not present") + "</span>";
    }).join("") + "</div>";
  }
  function renderNodes(tokens, active) {
    const width=94, height=29, gapX=6, gapY=6, startX=16, startY=53;
    return tokens.map((token,index) => {
      const col=index%3, row=Math.floor(index/3), x=startX+col*(width+gapX), y=startY+row*(height+gapY);
      const isActive=active.has(token);
      return '<g class="input-node' + (isActive ? " active" : "") + '" data-input="' + esc(token) +
        '" role="img" aria-label="' + esc(pretty[token] || token) + (isActive ? ", mapped in this context" : ", not mapped in this context") +
        '"><title>' + esc(token) + (isActive ? " — mapped in this context" : " — not mapped in this context") +
        '</title><rect x="' + x + '" y="' + y + '" width="' + width + '" height="' + height +
        '" rx="4"></rect><text x="' + (x+width/2) + '" y="' + (y+18) + '">' + esc(pretty[token] || token) + "</text></g>";
    }).join("");
  }
  function renderController(record) {
    const active = usedInputs(record);
    const names = Array.from(active).map((token) => pretty[token] || token);
    $("controller-map").innerHTML =
      '<svg viewBox="0 0 720 236" role="img" aria-labelledby="map-title map-desc"><title id="map-title">Logical controller input map for ' +
      esc(record.id) + '</title><desc id="map-desc">Abstract left and right input groups. Highlighted tokens are bound to actions required by this context. Marker positions do not represent hardware geometry.</desc>' +
      '<g><path class="housing-grip" d="M102 177 Q113 203 120 224 L242 224 Q250 203 262 177 Z"></path>' +
      '<rect class="housing" x="20" y="14" width="326" height="184" rx="88"></rect><text class="schematic-label" x="183" y="37" text-anchor="middle">LEFT INPUT SET</text>' +
      renderNodes(nodes.left,active) + '</g><g transform="translate(354,0)"><path class="housing-grip" d="M102 177 Q113 203 120 224 L242 224 Q250 203 262 177 Z"></path>' +
      '<rect class="housing" x="20" y="14" width="326" height="184" rx="88"></rect><text class="schematic-label" x="183" y="37" text-anchor="middle">RIGHT INPUT SET</text>' +
      renderNodes(nodes.right,active) + '</g></svg><div class="callout-key">' +
      (names.length ? names.map((n) => '<span class="active">' + esc(n) + "</span>").join("") : "<span>No bound physical tokens in these required actions.</span>") +
      '</div><p class="schematic-disclaimer">Schematic callouts group logical inputs by hand. Shape and marker placement are symbolic, not hardware geometry. Active inputs come from the effective action and axis export.</p>';
    $("controller-stage").classList.toggle("is-zoomed",zoomed);
    $("zoom-controller").textContent=zoomed ? "Collapse diagram −" : "Zoom diagram +";
    $("zoom-controller").setAttribute("aria-pressed",String(zoomed));
  }
  function postureEvidence(record) {
    const proof=data.posture;
    if (proof && record.id === proof.coverage_record_id) return postureLessonEvidence(record, proof);
    const lesson=data.field_lesson;
    if (lesson && record.id === lesson.coverage_record_id) return fieldLessonEvidence(lesson);
    return '<div class="limit-box"><strong>VIDEO / STILL</strong>No clip is attached to this situation. A listed binding does not by itself show what happens in the game.</div>';
  }
  function postureLessonEvidence(record, proof) {
    const before=proof.before || {}, after=proof.after || {}, input=proof.input_event;
    const sent=input ? (input.hand || "controller") + " controller " + (input.component || "input") + " input" : "an unrecorded controller input";
    const stance=(actionByName.get("gameplay.stance") || {}).label || "unknown";
    const runStance=(proof.run_stance_action || {}).label || "unknown";
    const routeNote=proof.run_stance_matches_current ? "The stance button mapping matches between the run record and the controls file shown here."
      : "The stance button mapping differs between the run record and the controls file shown here.";
    const hashNote=proof.controls_hash_matches_current ? "The selected controls-file snapshot matches the run."
      : "The selected controls-file snapshot differs, so the run mapping and current mapping are shown separately.";
    const observedLabel=proof.case_status === "observed_pass" ? "Observed in simulator" :
      (proof.case_status === "observed_failure" ? "Run reported failure" : "Run note available");
    return '<div class="evidence-box pending"><h3>Standing to crouch</h3><p><b>' + esc(observedLabel) +
      '.</b> One simulator run recorded Snake changing from standing to crouching after ' + esc(sent) +
      '. The run used <b>' + esc(runStance) + '</b> for stance; this guide lists <b>' + esc(stance) +
      '</b>. ' + esc(routeNote) + '</p><div class="evidence-facts"><span>VISUAL REVIEW PENDING</span><span>' +
      esc(proof.capture_eye || "LEFT").toUpperCase() + ' EYE ONLY</span><span>' +
      Number(proof.capture_fps || 0).toFixed(2) + ' FPS · DIAGNOSTIC</span><span>STEREO ' +
      (proof.stereo_acceptance ? "ACCEPTED" : "NOT ACCEPTED") + '</span></div><div class="evidence-actions">' +
      (proof.video_url ? '<button type="button" class="video-button" data-video-kind="posture">Play diagnostic clip ↗</button>' : "") +
      (proof.before_image_url ? '<a class="link-button" href="' + esc(proof.before_image_url) + '" target="_blank" rel="noreferrer">Before still ↗</a>' : "") +
      (proof.after_image_url ? '<a class="link-button" href="' + esc(proof.after_image_url) + '" target="_blank" rel="noreferrer">After still ↗</a>' : "") +
      '</div><details class="technical-note"><summary>Run details and source files</summary><p>Recorded game state: <code>STAND=' +
      esc(before.status_STAND) + ' / SQUAT=' + esc(before.status_SQUAT) + '</code> to <code>STAND=' +
      esc(after.status_STAND) + ' / SQUAT=' + esc(after.status_SQUAT) + '</code>. ' + esc(hashNote) +
      ' The wider stance and movement set is still marked <b>' +
      esc(outcomeName[record.native_outcome_status] || record.native_outcome_status) + '.</b></p><div class="evidence-actions">' +
      (proof.run_bindings_url ? '<a class="link-button" href="' + esc(proof.run_bindings_url) + '" target="_blank" rel="noreferrer">Run binding snapshot ↗</a>' : "") +
      (proof.identity_url ? '<a class="link-button" href="' + esc(proof.identity_url) + '" target="_blank" rel="noreferrer">Run identity ↗</a>' : "") +
      (proof.capture_url ? '<a class="link-button" href="' + esc(proof.capture_url) + '" target="_blank" rel="noreferrer">Capture details ↗</a>' : "") +
      (proof.result_url ? '<a class="link-button" href="' + esc(proof.result_url) + '" target="_blank" rel="noreferrer">Run record ↗</a>' : "") +
      '</div></details></div>';
  }
  function fieldLessonEvidence(proof) {
    const ack=proof.acknowledgment_note || "Input highlights follow controller command acknowledgments; a separate audit recorded the requested controls held.";
    const mappingNote=proof.controls_hash_matches_current
      ? "The run used the same controls-file snapshot selected above."
      : "The run used a different controls-file snapshot; the route above reflects the file selected now.";
    return '<div class="evidence-box pending"><h3>' + esc(proof.title || "Equip binoculars") + '</h3><p><b>Observed in simulator.</b> Snake equipped the binoculars after <b>' +
      esc(proof.input_label || "HOLD Y + L GRIP") + '</b>. This pass covers the equip action; eye alignment, zoom, target marking, and analysis remain unverified. ' + esc(mappingNote) + '</p>' +
      '<div class="evidence-facts"><span>VIDEO REVIEW PENDING</span><span>NATIVE SOURCE EYE + GAME AUDIO</span><span>' +
      Number(proof.source_fps || 0).toFixed(2) + ' FPS SOURCE SEGMENT · ' + Number(proof.source_dropped_frames || 0) + ' DROPS</span></div>' +
      '<p class="small-evidence-note">' + esc(ack) + '</p><div class="evidence-actions">' +
      (proof.video_url ? '<button type="button" class="video-button" data-video-kind="field-lesson">Play field clip ↗</button>' : "") +
      (proof.manifest_url ? '<a class="link-button" href="' + esc(proof.manifest_url) + '" target="_blank" rel="noreferrer">Clip timing & sources ↗</a>' : "") +
      (proof.controller_manifest_url ? '<a class="link-button" href="' + esc(proof.controller_manifest_url) + '" target="_blank" rel="noreferrer">Controller model & callouts ↗</a>' : "") +
      '</div></div>';
  }
  function renderDetail() {
    const record=chosenRecord;
    if (!record) return;
    const identity=record.identity || {};
    const missing=identity.missing || [];
    const contracts=record.required_actions.map((item) =>
      '<div class="action-contract"><p class="plain-copy">' + esc(item.intent) +
      '</p><p class="outcome-copy"><b>Expected result:</b> ' + esc(item.outcome) + "</p></div>").join("");
    const routeNames=actionsFor(record);
    const routeHtml=(routeNames.length ? routeNames.map(actionRoute).join("") :
      '<p class="plain-copy">No remappable control binding is listed for this situation.</p>') + directInputNote(record);
    $("detail").innerHTML =
      '<div class="detail-kicker"><p class="eyebrow">' + esc(record.game) + " / " +
      esc(familyName.get(record.family) || record.family) + '</p></div><h2>' + esc(title(record.id)) + '</h2><p class="detail-context">' + esc(record.context) +
      '</p><div class="status-row"><span class="tag">' + esc(bindingLabel(record)) +
      '</span><span class="status-pill ' + statusClass(record.native_outcome_status) + '">' +
      esc(outcomeName[record.native_outcome_status] || record.native_outcome_status) + '</span></div>' +
      '<section class="detail-section"><div class="section-head"><h3>Mapped controls</h3><span class="section-no">01</span></div><div class="routes">' + routeHtml + "</div></section>" +
      '<section class="detail-section"><div class="section-head"><h3>Find the inputs</h3><span class="section-no">02</span></div>' +
      '<div class="controller-wrap" id="controller-stage"><div class="controller-toolbar"><span>HIGHLIGHTED INPUTS ARE MAPPED HERE</span><button id="zoom-controller" class="zoom-button" type="button" aria-pressed="false">Zoom diagram +</button></div><div id="controller-map" class="controller-map"></div></div></section>' +
      '<details class="technical-note detail-section"><summary>Detailed case steps and expected results</summary><div class="action-list">' + contracts + "</div></details>" +
      '<section class="detail-section"><div class="section-head"><h3>Movement axes</h3><span class="section-no">03</span></div>' + renderAxes(record) + "</section>" +
      '<section class="detail-section"><div class="section-head"><h3>Leaving this situation</h3><span class="section-no">04</span></div>' +
      '<div class="limit-box"><strong>' + esc(record.exit_or_cancel && record.exit_or_cancel.intent || "No exit guidance recorded") + "</strong>" +
      esc(record.exit_or_cancel && record.exit_or_cancel.outcome || "") + "</div></section>" +
      '<section class="detail-section"><div class="section-head"><h3>Run & video notes</h3><span class="section-no">05</span></div>' +
      postureEvidence(record) + '</section>' +
      '<details class="technical-note detail-section"><summary>Entry requirements, identity, and source references</summary>' +
      '<p class="plain-copy">Situation reference: <code>' + esc(record.id) + '</code> · source identity status: <code>' +
      esc(identity.status || "unknown") + '</code></p>' +
      renderPredicate(identity) + (missing.length ? '<ul class="missing-list">' + missing.map((x) => "<li>" + esc(x) + "</li>").join("") + "</ul>" :
        '<p class="plain-copy">No unresolved identity fields are listed; other outcome checks may remain open.</p>') +
      '<div class="source-links"><span class="small-cap">SOURCE REFERENCES</span><p>' +
      (record.evidence_refs || []).map((x) => "<code>" + esc(x) + "</code>").join(" · ") +
      '</p></div></details>';
    renderController(record);
    $("zoom-controller").addEventListener("click", () => {
      zoomed=!zoomed;
      $("controller-stage").classList.toggle("is-zoomed",zoomed);
      $("zoom-controller").textContent=zoomed ? "Collapse diagram −" : "Zoom diagram +";
      $("zoom-controller").setAttribute("aria-pressed",String(zoomed));
    });
  }
  function openVideo(kind) {
    activeVideoKind=kind === "field-lesson" ? "field-lesson" : "posture";
    const isFieldLesson=activeVideoKind === "field-lesson";
    const proof=isFieldLesson ? data.field_lesson : data.posture;
    if (!proof || !proof.video_url) return;
    const modal=$("video-modal"), video=$("evidence-video");
    $("video-title").textContent=isFieldLesson ? (proof.title || "Equip binoculars") : "Standing to crouch — diagnostic clip";
    $("video-warning").textContent=isFieldLesson
      ? "Native source-eye excerpt from a " + Number(proof.source_seconds || 0).toFixed(1) + " second segment at " + Number(proof.source_fps || 0).toFixed(2) + " FPS; the source segment logged " + Number(proof.source_dropped_frames || 0) + " drops. Clip review is pending; this pass shows equip only."
      : "Left-eye diagnostic at " + Number(proof.capture_fps || 0).toFixed(2) + " measured FPS. Visual review is pending; this is diagnostic footage.";
    $("video-caption").textContent=isFieldLesson
      ? (proof.source_kind || "Native source eye") + " · game audio included · command-acknowledgment input cues · held-input audit recorded separately."
      : (proof.capture_source || "Source not specified") + " · " + (proof.capture_eye || "eye not specified") +
        " eye · " + Number(proof.capture_duration || 0).toFixed(1) + " seconds. Stereo acceptance: " +
        (proof.stereo_acceptance ? "yes" : "no") + "; full-mod acceptance: " + (proof.full_mod_acceptance ? "yes" : "no") + ".";
    video.src=proof.video_url;
    video.poster=proof.after_image_url || "";
    modal.hidden=false;
    $("video-close").focus();
    video.play().catch(() => {});
  }
  function closeVideo() {
    const video=$("evidence-video");
    video.pause(); video.removeAttribute("src"); video.load();
    $("video-modal").hidden=true;
    document.querySelector('[data-video-kind="' + activeVideoKind + '"]')?.focus();
  }
  $("mode-list").addEventListener("click",(event) => {
    const button=event.target.closest("[data-family]");
    if (!button) return;
    chosenFamily=button.dataset.family;
    renderModes(); renderRecords();
  });
  $("search").addEventListener("input",renderRecords);
  $("game-filter").addEventListener("change",renderRecords);
  $("observed-filter").addEventListener("change",renderRecords);
  $("video-modal").querySelectorAll("[data-close-video]").forEach((button) => button.addEventListener("click",closeVideo));
  $("video-close").addEventListener("click",closeVideo);
  $("detail").addEventListener("click",(event) => {
    const button=event.target.closest("[data-video-kind]");
    if (button) openVideo(button.dataset.videoKind);
  });
  document.addEventListener("keydown",(event) => { if (event.key === "Escape" && !$("video-modal").hidden) closeVideo(); });
  $("obligation-count").textContent=records.length;
  $("controls-source").textContent=(data.metadata && data.metadata.controls_config_display) || "source not recorded";
  if (data.action_catalog) {
    const catalog=data.action_catalog;
    $("catalog-summary").textContent="This separate TPP reference lists " + catalog.loadout_position_count +
      " loadout positions across weapons, support gear, and items, and tracks " + catalog.tracked_state_family_count +
      " gameplay-state families. " + catalog.direct_readback_family_count +
      " families currently have direct state readbacks. A listed slot or readback does not show that every action is usable or covered.";
  }
  $("build-stamp").textContent=records.length + " SITUATIONS CATALOGUED · " +
    (data.coverage.updated || "UNDATED") + " INVENTORY";
  renderModes();
  renderRecords();
})();
