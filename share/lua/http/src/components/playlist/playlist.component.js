Vue.component('playlist', {
    template: '#playlist-template',
    computed: {
        ...Vuex.mapState({
            playlist: state => state.playlist.items
        }),
    },
    methods: {
        addItem(mode, id, title, src) {
            this.$store.dispatch('playlist/addItem', src);
        },
        removeItem(id) {
            this.$store.dispatch('playlist/removeItem', id);
        },
        fetchPlaylist() {
            this.$store.dispatch('playlist/fetchPlaylist');
        },
        openPlaylist() {
            $('#playlistNav').width('60%');
            $('#mobilePlaylistNavButton').width('0%');
        },
        closePlaylist() {
            $('#playlistNav').width('0%');
            $('#mobilePlaylistNavButton').width('10%');
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
        },
        play(src, id) {
            this.$store.dispatch('status/play', id);
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
