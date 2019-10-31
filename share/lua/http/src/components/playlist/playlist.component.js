Vue.component('playlist', {
    template: '#playlist-template',
    computed: {
        ...Vuex.mapState({
            playlist: state => state.playlist,
        }),
    },
    methods: {
        fetchPlaylist() {
            this.$store.dispatch('playlist/fetchPlaylist');
        },
        refreshPlaylist() {
            if (this.interval) {
                clearTimeout(this.interval);
                this.interval = null;
            }
            this.fetchPlaylist();
            this.interval = setInterval(() => {
                this.fetchPlaylist();
            }, 5000);
        }
    },
    created() {
        this.$store.subscribeAction((mutation) => {
            switch (mutation.type) {
                case 'layout/openPlaylist':
                    this.openPlaylist();
                    break;
                case 'layout/closePlaylist':
                    this.closePlaylist();
                    break;
            }
        });
        this.refreshPlaylist();
    }
});
