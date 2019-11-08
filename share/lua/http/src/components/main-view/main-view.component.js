Vue.component('main-view', {
    template: '#main-view-template',
    computed: {
        ...Vuex.mapState({
            mainView: state => state.layout.mainView,
        })
    },
    methods: {
        setLibraryView() {
            this.$store.dispatch('layout/setLibraryView');
        }
    },
    created() {
        this.mainView === 'player' ? document.body.style['overflow-y'] = 'hidden' :
            document.body.style['overflow-y'] = 'auto';
        this.$store.subscribeAction((action, payload) => {
            switch (action.type) {
                case 'layout/setLibraryView':
                    document.body.style['overflow-y'] = 'auto';
                    break;
                case 'layout/setPlayerView':
                    document.body.style['overflow-y'] = 'hidden';
                    break;
            }
        });
    }
});
