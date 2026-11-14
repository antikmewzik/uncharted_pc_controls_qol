/* U3, horseback, PAD ALONE: no turn after the aim is let go. Included by runtime_pad.c, before u3_script_layer.
 *
 * Standalone module: it does nothing unless pad_only_mode is on (the controller is in use), so the cut of all our other camera changes with the pad,
 * which keeps the game's native behaviour, is left as it is. BB_U3_HORSEPIN=0: off.
 *
 * Measured 2026-11-14 (Misc\last_run_u3_cheval_manette1.log, ..._native0.log): at the release the climb-class layer 0x1671390 (the horse's walking
 * picture, the walking camera's own object) fades in over ~0.4 s from the game's own heading, 12-18 degrees off the aim's last picture. The view
 * turned by itself, the same with BB_U3_NATIVE=0. The layer's pose is held on the picture rendered at the release, and the camera's direction is
 * turned toward it until they agree. The right stick moves the held picture by a model of its own rate; the mouse gives it back.
 *
 * Needs from runtime_pad.c: pad_only_mode, aim_held, u3_anim_given, wrap_degrees, u3_turn_pose, u3_anim_pivot, bbgpu_camera_yaw, quat_rotate, safe_read.
 * Hook: u3_horsepin_layer(r14), called at the top of the layer hook. Feed: u3_hp_release_us (release of the aim), u3_hp_stick (right stick moved),
 * both set where the pad state is read. */
#define BB_HP_GAIN 1.15f   /* (a little over the fit so that the hold never brakes the stick) */
static int u3_hp_stick, u3_hp_follow, u3_hp_rx, u3_hp_ry, u3_hp_aimstick;
static float u3_hp_arc; static int u3_hp_arc_ok;
static void u3_hp_arc_hold(uint64_t r14) {   /* the vertical angle (the walking camera's arc) is kept too: the quick cycles let it drift a little (measured by Wiktor, spam of L2). A stick move on Y gives it back. */
    float arc=0.0f;
    if (!safe_read(&arc,r14+0x7874,4) || !(arc>-3.0f && arc<3.0f)) return;
    const int ystick=(u3_hp_ry>148 || u3_hp_ry<108);
    if (!u3_hp_arc_ok || ystick) { u3_hp_arc=arc; u3_hp_arc_ok=1; return; }
    if (arc-u3_hp_arc>0.0005f || arc-u3_hp_arc<-0.0005f) { *(float *)(uintptr_t)(r14+0x7874)=u3_hp_arc; if (mouse_debug()) printf("Mouse: U3 HORSEPIN arc %.4f -> %.4f t=%.4f\n",(double)arc,(double)u3_hp_arc,now_us()*1e-6); }
}
static float u3_hp_target; static int u3_hp_ok; static uint64_t u3_hp_key, u3_hp_release_us; static double u3_hp_given; 
static int u3_hp_active(uint64_t now) {
    static int on=-1; if (on<0) { const char *e=getenv("BB_U3_HORSEPIN"); on=!(e && e[0]=='0'); }
    if (!on || !pad_only_mode || aim_held || !u3_hp_release_us || now<=u3_hp_release_us || now-u3_hp_release_us>=1000000) return 0;
    if (u3_hp_key!=u3_hp_release_us) {
        const int chain=u3_hp_ok && u3_hp_key && u3_hp_release_us-u3_hp_key<2000000 && !u3_hp_aimstick;   /* (a quick re-press with the stick untouched during the aim: the picture shown at this release is the held one plus ~3 degrees of the aim's own ease, which added up over a spam of L2; the first target is kept) */
        u3_hp_key=u3_hp_release_us; u3_hp_follow=0; float ry=0.0f; double ra=0.0;
        if (!chain) u3_hp_arc_ok=0;
        if (chain) { u3_hp_aimstick=0; u3_hp_given=u3_anim_given; if (mouse_debug()) printf("Mouse: U3 HORSEPIN chain: target kept %.2f t=%.4f\n",(double)u3_hp_target,now*1e-6); return 1; }
        u3_hp_aimstick=0;
        u3_hp_ok=now-u3_hp_release_us<300000 && bbgpu_camera_yaw(&ry,&ra) && ra<0.1 && ry==ry; u3_hp_target=ry; u3_hp_given=u3_anim_given;
        if (mouse_debug()) printf("Mouse: U3 HORSEPIN start ok=%d target=%.2f t=%.4f\n",u3_hp_ok,(double)ry,now*1e-6);
    }
    if (u3_hp_ok && u3_anim_given!=u3_hp_given) u3_hp_ok=0;
    return u3_hp_ok;
}
static void u3_hp_turn_camera(uint64_t r14, float deg) {   /* the camera object itself: its direction and shown position turned about the pivot (the game's heading goes down by deg) */
    const float a=deg*0.017453293f, c=SDL_cosf(a), s=SDL_sinf(a);
    float *d=(float *)(uintptr_t)(r14+0x78c0), *pos=(float *)(uintptr_t)(r14+0x7680);
    const float *pivot=(const float *)(uintptr_t)(r14+0x78a0);
    const float px=pos[0]-pivot[0], pz=pos[2]-pivot[2], nx=pivot[0]+px*c-pz*s, nz=pivot[2]+px*s+pz*c, x=d[0], z=d[2];
    if (nx>-100000.0f && nx<100000.0f && nz>-100000.0f && nz<100000.0f && x>-1.5f && x<1.5f && z>-1.5f && z<1.5f) { pos[0]=nx; pos[2]=nz; d[0]=x*c-z*s; d[2]=x*s+z*c; }
}
static void u3_horsepin_layer(uint64_t r14) {    uint64_t vt=0; float w=0.0f;
    const uint64_t now=now_us();
    if (!pad_only_mode || !safe_read(&vt,r14,8) || vt!=runtime_image_base()+0x1671390 || !safe_read(&w,r14+0x108,4) || !(w>0.0005f && w<=1.0001f)) return;
    const RuntimeGuestRegs *g=(const RuntimeGuestRegs *)(uintptr_t)(guest_callback_rsp-0x60);
    float *p=(float *)(uintptr_t)g->rax;
    if ((uintptr_t)p<0x10000) return;
    const float len2=p[4]*p[4]+p[5]*p[5]+p[6]*p[6]+p[7]*p[7];
    if (!(len2>0.98f && len2<1.02f && p[0]>-100000.0f && p[0]<100000.0f && p[1]>-100000.0f && p[1]<100000.0f && p[2]>-100000.0f && p[2]<100000.0f)) return;
    const float fw[3]={0.0f,0.0f,1.0f}; float f[3]; quat_rotate(p+4,fw,f);
    const float hraw=SDL_atan2f(f[0],f[2])*57.29578f;
    if (!(hraw==hraw)) return;
    static float prev_raw; static uint64_t prev_us;
    const uint64_t prev_us_old=prev_us; const int have_prev=prev_us && now>prev_us && now-prev_us<100000; const float draw=have_prev ? wrap_degrees(hraw-prev_raw) : 0.0f; prev_raw=hraw; prev_us=now;
    /* (L2 pressed again during the hold: the aiming camera starts from the walking camera's heading, which the game has been easing away from the held picture; each quick cycle then added 7-9 degrees to the turn, measured 2026-11-14 on a spam of L2. The camera's heading is put on the target at once, so the next aim starts from the picture shown) */
    if (aim_held && u3_hp_ok && u3_hp_release_us && u3_hp_key==u3_hp_release_us && now>u3_hp_release_us && now-u3_hp_release_us<1500000) {
        u3_hp_arc_hold(r14);
        const float d0=wrap_degrees(hraw-u3_hp_target);
        if ((d0>0.1f || d0<-0.1f) && d0<40.0f && d0>-40.0f) { u3_hp_turn_camera(r14,d0); if (mouse_debug()) printf("Mouse: U3 HORSEPIN repress: camera put on target (%.2f -> %.2f) t=%.4f\n",(double)hraw,(double)u3_hp_target,now*1e-6); }
        return;
    }
    if (!u3_hp_active(now)) return;
    /* (measured 2026-11-14, last_run_u3_cheval_manette10.log: the game's heading after the release is NOT only the stick: it eases toward the horse's own heading at up to ~190 deg/s (the native recentre, which is the turn by itself), and it is the same whether the stick is touched or not. Following the heading's change (the first try) let that ease through as soon as the stick moved. The stick's own turn is modelled from its position instead (left = heading up; rate = 0.0346*deflection^1.6 deg/s, fitted on the log: 8 deg/s at deflection 30, 20 at 53), the target moves by it, and the game's heading is always brought back to the target, so the recentre never shows.) */
    if (u3_hp_stick) { const float dt=have_prev ? (float)(now-prev_us_old)*1e-6f : 0.0f; float dx=(float)(128-u3_hp_rx); const float ax=dx<0.0f ? -dx : dx; const float rate=ax>4.0f ? 0.0346f*powf(ax-4.0f+4.0f,1.6f)*(BB_HP_GAIN) : 0.0f; u3_hp_target=wrap_degrees(u3_hp_target+(dx<0.0f ? -rate : rate)*(dt>0.05f ? 0.05f : dt)); if (mouse_debug()) printf("Mouse: U3 HORSEPIN stick: rx=%d draw=%.2f target=%.2f t=%.4f\n",u3_hp_rx,(double)draw,(double)u3_hp_target,now*1e-6); }
    u3_hp_arc_hold(r14);
    const float dh=wrap_degrees(hraw-u3_hp_target);
    if (dh<=0.05f && dh>=-0.05f) return;
    if (dh>40.0f || dh<-40.0f) { u3_hp_ok=0; return; }
    /* (measured 2026-11-14: by the time the layer is at full weight the game's heading has settled within ~1 degree of the target, with or without the stick. The hold is not kept for the whole second, which showed as a view held, then freed: it is let go over 0.25 s from the first full-weight frame) */
    static uint64_t full_since, full_key; float k=1.0f;
    if (full_key!=u3_hp_key) { full_key=u3_hp_key; full_since=0; }
    if (w>=0.995f) { if (!full_since) full_since=now; const float el=(float)(now-full_since)*1e-6f; if (el>=0.25f) { u3_hp_ok=0; return; } k=1.0f-el/0.25f; } else full_since=0;
    float pv[3]={p[0],p[1],p[2]}; u3_anim_pivot(p,pv);
    u3_turn_pose(p,pv,dh*k,0.0f);   /* the picture stays where the aim left it */
    u3_hp_turn_camera(r14,0.5f*dh);   /* so that the game's heading comes to the target */
    if (mouse_debug()) printf("Mouse: U3 HORSEPIN layer w=%.2f raw=%.2f target=%.2f t=%.4f\n",(double)w,(double)hraw,(double)u3_hp_target,now*1e-6);
}
