// Mode notes describe runtime ownership. Button names come from bindingsJson().
export const modeInfo={
 gameplay:['On foot','Walk, turn, use physical weapons and interact. Opening a menu gives its controls priority.'],
 menus:['iDroid & menus','The iDroid is in your LEFT bionic hand. Hold its focus binding to use the menus: both sticks and face buttons belong to the iDroid. Release focus to return to movement where the native screen permits it. Pause and blocking menus retain menu control.'],
 equipment:['Equipment picker','Hold the equipment binding to open the LEFT-arm picker. Choose a category, center the browsing stick, then choose a card. Release to equip and close.'],
 commands:['Buddy commands','Open Commands while keeping its hold binding down. A weapon or CQC hold already in progress can continue; new attacks are suppressed while choosing.'],
 binoculars:['Binoculars','The physical binoculars own these controls even while lowered. Raise the ocular to your eye, aim by hand, zoom and mark.'],
 horse:['Horse','Ride using the movement stick. Weapon-ready, firing and equipment remain contextual; stow the weapon before dismounting.'],
 vehicle:['Vehicles','An empty hand near the wheel steers while gripped. Both hands can steer together. Native vehicle weapon and camera controls remain contextual. Each control has one owner at a time.'],
 nativeButtons:['Native controls','Optional native gamepad input mode. Ordinary VR action mappings are dormant. The native game still decides which actions are available.']
};
export const modeAxes={gameplay:['move','turn'],menus:['menu','map'],equipment:['equipment'],commands:['commands'],binoculars:['move','turn'],horse:['move','equipment'],vehicle:['vehicle_steering','vehicle_pedals','equipment'],nativeButtons:['native_move','native_look']};
export function axisTitle(name,mode){
 if(name==='axes.equipment'&&mode==='vehicle')return 'Camera / turret · both axes';
 if(name==='axes.equipment'&&mode==='horse')return 'Mounted camera · both axes';
 if(name==='axes.vehicle_steering')return 'Steering · horizontal axis';
 if(name==='axes.vehicle_pedals')return 'Gun-ready pedals · vertical axis';
 return name.replace('axes.','').replaceAll('_',' ')+' · stick axis';
}
const descriptions={
 'system.idroid':'Open or stow the LEFT-hand iDroid. Hold the separate focus binding to operate its screen.',
 'system.idroid_focus':'Hold to focus an open handheld iDroid. Its sticks, face buttons and menu controls take ownership; ordinary movement, turning and weapon chords are dormant. This is not Previous tab.',
 'system.pause':'Open Pause, or the native Help action while in iDroid. Pause uses the spatial stereo panel.',
 'system.recenter':'Recenter tracked position without changing your facing. The longer chord consumes its simpler Menu action.',
 'system.native_buttons':'Toggle complete native gamepad input. Release the chord before using the newly selected mode.',
 'gameplay.ready_weapon':'Hold the physical gun ready. Release it to lower the weapon.',
 'gameplay.zoom':'While the gun is ready, cycle its fitted optic powers. SVG-76 with the fitted magnifier cycles 1× / 2×. Fixed-power optics keep their one power. Center the stick before repeating.',
 'gameplay.support_grip':'Squeeze near the physical support socket to support the weapon. In empty space this only curls your fingers.',
 'gameplay.switch_weapon':'Use the complete chord while the weapon is ready. Its stick click does not also dive.',
 'gameplay.reload':'Use the complete reload chord. It consumes the simpler pickup action for that input.',
 'gameplay.fire_or_cqc':'A readied weapon fires or throws; with the weapon lowered, the native game supplies CQC or attack. Weapon and stance restrictions still apply.',
 'gameplay.equip_binoculars':'Equip physical binoculars into the right hand. Release the chord, then raise the ocular to your eye. The chord does not also interact.',
 'equipment.open':'Hold to open the LEFT-arm category picker. Release without choosing to cancel, or after choosing to equip.',
 'commands.open':'Keep the Commands hold binding down while navigating. A pre-existing ready/CQC hold can continue.',
 'binoculars.zoom':'Cycle binocular lens power while the physical binoculars are selected. This does not zoom the surrounding world.',
 'binoculars.mark':'Explicitly mark the visible target under the hand-aimed sight. The automatic-mark setting is separate.',
 'menus.previous_tab':'Previous page in Pause and native menus. Handheld iDroid uses its separate Previous tab chord so the focus binding stays dedicated.',
 'idroid.previous_tab':'While handheld iDroid is focused, flick the right stick left for the previous tab. Center before repeating; keep focus held. The left-stick click alternative remains. Tab inputs do not also move the menu cursor.',
 'idroid.next_tab':'While handheld iDroid is focused, flick the right stick right for the next tab. Center before repeating; keep focus held. The separate right-grip page loop remains available.',
 'menus.next_tab':'In focused handheld iDroid, each fresh press advances one page. Release before repeating; the root pages wrap from last to first. A grip already carrying a gun does not also change tabs.',
 'menus.left_trigger':'Native menu trigger / map zoom while focused. When an already-held gun is carried into iDroid focus, trigger zoom is dormant.',
 'menus.right_trigger':'Native menu trigger / map zoom while focused. When an already-held gun is carried into iDroid focus, this trigger belongs to the gun and cannot also zoom.',
 'menus.left_click':'Native menu stick-click action. It does not also sprint while the iDroid owns input.',
 'menus.right_click':'Native menu stick-click / zoom-step action. It does not also dive or switch weapons while the iDroid owns input.',
 'vehicle.wheel_grip':'Squeeze with the LEFT hand near the wheel, then rotate the hand to steer. Release to detach.',
 'vehicle.right_wheel_grip':'With the right hand empty, squeeze near the wheel to join steering. Wheel contact owns that grip instead of drawing a gun.',
 'vehicle.ready_weapon':'Away from the wheel, hold to request a personal gun where the native vehicle and seat permit it. Wheel contact owns a squeeze until released.',
 'vehicle.fire':'Fire the readied personal gun. While it owns this trigger, acceleration uses the vehicle pedals axis.',
 'vehicle.reload':'Reload a readied personal gun using this complete binding. This does not also open equipment or perform an on-foot action.',
 'vehicle.accelerate':'Accelerate while the gun is stowed. With a personal gun ready, the vertical vehicle pedals axis owns acceleration instead.',
 'vehicle.brake_reverse':'Brake or reverse while the gun is stowed. With a personal gun ready, the vertical vehicle pedals axis owns braking and reverse instead.',
 'vehicle.equipment_open':'Optional vehicle equipment picker. Disabled by default so a bare right grip cannot unexpectedly open it. Any custom binding must stay separate from the wheel.',
 'vehicle.interact':'Enter or exit when the native contextual prompt is available. Stow a readied gun before exiting.'
};
export function actionDescription(action){return descriptions[typeof action==='string'?action:action.name]||(action.modifier?'Hold this binding to use this mode’s controls.':'Available when this mode owns input. The game still controls contextual availability.');}
