/* Opt-in hardware diagnostic, excluded from normal builds. It changes only
   DisplayGrid's live objects; WdgLoader and saved layouts are not edited. */
static void views_device_probe(unsigned long long now) {
    static unsigned long long next;
    static int stage,done;
    static void *grid;
    static void (*clear_grid)(void *);
    static void *(*book)(void),*(*active)(void *);
    static const struct rect *(*position)(void *);
    static unsigned int (*size_of)(void *,int);
    if(done || !runtime_host)return;
    if(!next){next=now+2000000;return;}
    if(now<next)return;
    next=now+700000;
    if(!grid) {
        *(void **)(&book)=runtime_host->resolve(runtime_host->official_library,"_ZN10WidgetBook8instanceEv");
        *(void **)(&active)=runtime_host->resolve(runtime_host->official_library,"_ZNK10WidgetBook13getActiveGridEv");
        *(void **)(&clear_grid)=runtime_host->resolve(runtime_host->official_library,"_ZN11DisplayGrid14clearActivatedEv");
        *(void **)(&size_of)=runtime_host->resolve(runtime_host->official_library,"_ZN11DisplayGrid13getWidgetSizeE12WdgOutlook_t");
        *(void **)(&position)=runtime_host->resolve(runtime_host->ui_runtime,"_ZNK6ZKBase11getPositionEv");
        if(!book || !active || !clear_grid || !size_of || !position){runtime_host->log(runtime_host,"widget-check: FAIL symbols");done=1;return;}
        grid=active(book());if(!grid){runtime_host->log(runtime_host,"widget-check: FAIL active grid");done=1;return;}
    }
    clear_grid(grid);views_tick(now);
    if(stage==8){runtime_host->log(runtime_host,"widget-check: PASS all 8 native sizes; no saved layout changes");done=1;return;}
    const struct widget_provider *example=0;
    for(int i=0;i<PROVIDERS;++i)if(providers[i].host && providers[i].source!=FIRST_SOURCE){example=&providers[i];break;}
    if(!example){runtime_host->log(runtime_host,"widget-check: FAIL example missing");done=1;return;}
    unsigned short sources[3]={(unsigned short)example->source,0,35};
    const struct widget_geometry *g=&widget_sizes[stage];
    struct layout layout={.type=g->type,.id=(unsigned short)(60000+stage),.begin=sources,.end=sources+3,.capacity=sources+3};
    void *widget=activate_widget(grid,&layout,0);struct view *v=native_view(widget);
    unsigned int size=size_of(grid,g->type);
    const struct rect *r=v?position(v->public.window):0;
    int ok=v && r && r->x==0 && r->y==11 && r->w==g->width && r->h==g->height &&
        (size&65535)==(unsigned int)g->columns && (size>>16)==(unsigned int)g->rows &&
        get_by_button(grid,get_id(v->button))==widget && !video_layout(grid) && registry_source(widget)==example->source;
    if(ok) {
        views_tick(now);ok=v->attached && !v->failed;
        set_editable(widget,1);views_tick(now);ok=ok && v->public.editing && !is_visible(v->public.action);
        set_editable(widget,0);views_tick(now);ok=ok && !v->public.editing && is_visible(v->public.action);
    }
    char message[160];snprintf(message,sizeof(message),"widget-check: %s %dx%d (%dx%d), native lookup, edit toggle, video exclusion",ok?"PASS":"FAIL",g->columns,g->rows,g->width,g->height);
    runtime_host->log(runtime_host,message);
    if(!ok){done=1;return;}
    if(stage==0) {
        extern int registry_device_probe(void *);
        struct rect saved_position=*position(v->public.window);
        ok=registry_device_probe(v->public.window);
        set_position(v->public.window,&saved_position);
        if(!ok){done=1;return;}
        layout.x=1;layout.id=61000;void *second=activate_widget(grid,&layout,0);
        unsigned short car_sources[3]={FIRST_SOURCE,0,35};layout=(struct layout){.type=5,.x=2,.id=62000,.begin=car_sources,.end=car_sources+3,.capacity=car_sources+3};
        void *car=activate_widget(grid,&layout,0);views_tick(now);
        struct view *other=native_view(second),*vehicle=native_view(car);
        ok=other && vehicle && other!=v && vehicle!=v && other->public.user!=v->public.user && other->attached && vehicle->attached && !vehicle->failed;
        runtime_host->log(runtime_host,ok?"widget-check: PASS two examples and car coexist":"widget-check: FAIL coexistence");
        if(!ok){done=1;return;}
    }
    ++stage;
}
